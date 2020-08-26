#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pru_cfg.h>
#include <pru_iep.h>
#include <pru_intc.h>
#include <rsc_types.h>

#include "gpio.h"
#include "intc.h"
#include "resource_table_def.h"
#include "rpmsg.h"
#include "simple_lock.h"

#include "commons.h"
#include "hw_config.h"
#include "ringbuffer.h"
#include "sampling.h"
#include "shepherd_config.h"
#include "virtcap.h"
/* Used to signal an invalid buffer index */
#define NO_BUFFER 0xFFFFFFFF

static struct RingBuffer free_buffers;

enum int_source_e { SIG_SAMPLE, SIG_BLOCK_END } int_source;

volatile struct SharedMem *shared_mem =
	(volatile struct SharedMem *)PRU_SHARED_MEM_STRUCT_OFFSET;

static void send_message(unsigned int msg_id, unsigned int value)
{
	struct DEPMsg msg_out;

	msg_out.msg_type = msg_id;
	msg_out.value = value;
	rpmsg_putraw((void *)&msg_out, sizeof(struct DEPMsg));
}

unsigned int handle_block_end(volatile struct SharedMem *shared_mem,
			      struct RingBuffer *free_buffers_ptr,
			      struct SampleBuffer *buffers,
			      unsigned int current_buffer_idx,
			      unsigned int sample_idx)
{
	unsigned int next_buffer_idx;
    uint8_t tmp_idx;
	struct SampleBuffer *next_buffer;

	/* Lock access to gpio_edges structure to avoid inconsistency */
	simple_mutex_enter(&shared_mem->gpio_edges_mutex);

	/* If we currently have a valid buffer, return it to host */
	if (current_buffer_idx != NO_BUFFER) {
		if (sample_idx != SAMPLES_PER_BUFFER)
			send_message(MSG_DEP_ERR_INCMPLT, sample_idx);

		(buffers + current_buffer_idx)->len = sample_idx;
		send_message(MSG_DEP_BUF_FROM_PRU, current_buffer_idx);
	}

	/* Fetch new buffer from ring */
	if (ring_get(free_buffers_ptr, &tmp_idx) == 0) {
		next_buffer_idx = (unsigned int)tmp_idx;
		next_buffer = buffers + next_buffer_idx;
		next_buffer->timestamp_ns = shared_mem->next_timestamp_ns;
		next_buffer->gpio_edges.idx = 0;
		shared_mem->gpio_edges = &next_buffer->gpio_edges;
	} else {
		next_buffer_idx = NO_BUFFER;
		shared_mem->gpio_edges = NULL;
		send_message(MSG_DEP_ERR_NOFREEBUF, 0);
	}
	simple_mutex_exit(&shared_mem->gpio_edges_mutex);

	return next_buffer_idx;
}

int handle_rpmsg(struct RingBuffer *free_buffers_ptr, enum ShepherdMode mode,
		 enum ShepherdState state)
{
	struct DEPMsg msg_in;

	/*
	 * TI's implementation of RPMSG on the PRU triggers the same interrupt
	 * line when sending a message that is used to signal the reception of
	 * the message. We therefore need to check the length of the potential
	 * message
	 */
	if (rpmsg_get((void *)&msg_in) != sizeof(struct DEPMsg))
		return 0;

	_GPIO_TOGGLE(USR_LED1);

	if ((mode == MODE_DEBUG) && (state == STATE_RUNNING)) {
		unsigned int res;
		switch (msg_in.msg_type) {
		case MSG_DEP_DBG_ADC:
			res = sample_dbg_adc(msg_in.value);
			send_message(MSG_DEP_DBG_ADC, res);
			return 0;

		case MSG_DEP_DBG_DAC:
			sample_dbg_dac(msg_in.value);
			return 0;

		default:
			send_message(MSG_DEP_ERR_INVLDCMD, msg_in.msg_type);
			return -1;
		}
	} else {
		if (msg_in.msg_type == MSG_DEP_BUF_FROM_HOST) {
			ring_put(free_buffers_ptr, (uint8_t)msg_in.value);
			return 0;
		} else {
			send_message(MSG_DEP_ERR_INVLDCMD, msg_in.msg_type);
			return -1;
		}
	}
}

void event_loop(volatile struct SharedMem *shared_mem,
		struct RingBuffer *free_buffers_ptr,
		struct SampleBuffer *buffers)
{
	unsigned int sample_idx = 0;
	unsigned int buffer_idx = NO_BUFFER;

	enum ShepherdMode shepherd_mode =
		(enum ShepherdMode)shared_mem->shepherd_mode;

	while (1) {
		/* Check if a sample was triggered by PRU1 */
		if (read_r31() & (1U << 31)) {
			/* Important: We have to clear the interrupt here, to avoid missing interrupts */
			if (INTC_CHECK_EVENT(PRU_PRU_EVT_BLOCK_END)) {
				int_source = SIG_BLOCK_END;
				INTC_CLEAR_EVENT(PRU_PRU_EVT_BLOCK_END);
			} else if (INTC_CHECK_EVENT(PRU_PRU_EVT_SAMPLE)) {
				int_source = SIG_SAMPLE;
				INTC_CLEAR_EVENT(PRU_PRU_EVT_SAMPLE);
			} else {
				continue;
			}

			/* The actual sampling takes place here */
			if (buffer_idx != NO_BUFFER) {
				sample(buffers + buffer_idx, sample_idx++,
				       shepherd_mode);
			}

			if (int_source == SIG_BLOCK_END) {
				/* Did the Linux kernel module ask for reset? */
				if (shared_mem->shepherd_state == STATE_RESET) {
					return;
				}

				/* We try to exchange a full buffer for a fresh one if we are running */
				if ((shared_mem->shepherd_state ==
				     STATE_RUNNING) &&
				    (shared_mem->shepherd_mode != MODE_DEBUG))
					buffer_idx = handle_block_end(
						shared_mem, free_buffers_ptr,
						buffers, buffer_idx,
						sample_idx);

				sample_idx = 0;
				_GPIO_OFF(USR_LED1);
			}
			/* We only handle rpmsg comms if we're not at the last sample */
			else {
				handle_rpmsg(free_buffers_ptr,
					     (enum ShepherdMode)
						     shared_mem->shepherd_mode,
					     (enum ShepherdState)shared_mem
						     ->shepherd_state);
			}
		}
	}
}

int main(void)
{
	/*
	 * The shared mem is dynamically allocated and we have to inform user space
	 * about the address and size via sysfs, which exposes parts of the
	 * shared_mem structure.
	 * Do this initialization early! The kernel module relies on it.
	 */
	shared_mem->mem_base_addr = resourceTable.shared_mem.pa;
	shared_mem->mem_size = resourceTable.shared_mem.len;

	shared_mem->n_buffers = RING_SIZE;
	shared_mem->samples_per_buffer = SAMPLES_PER_BUFFER;
	shared_mem->buffer_period_ns = BUFFER_PERIOD_NS;

	shared_mem->harvesting_voltage = 0;
	shared_mem->shepherd_mode = MODE_HARVESTING;

	/*
	 * The dynamically allocated shared DDR RAM holds all the buffers that
	 * are used to transfer the actual data between us and the Linux host.
	 * This memory is requested from remoteproc via a carveout resource request
	 * in our resourcetable
	 */
	struct SampleBuffer *buffers =
		(struct SampleBuffer *)resourceTable.shared_mem.pa;

	/* Allow OCP master port access by the PRU so the PRU can read external memories */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

	simple_mutex_exit(&shared_mem->gpio_edges_mutex);

	/* Enable interrupts from PRU1 */
	CT_INTC.EISR_bit.EN_SET_IDX = PRU_PRU_EVT_SAMPLE;
	CT_INTC.EISR_bit.EN_SET_IDX = PRU_PRU_EVT_BLOCK_END;

	rpmsg_init("rpmsg-pru");

	_GPIO_OFF(USR_LED1);
	_GPIO_OFF(DEBUG_P0);

	while (true) {
		_GPIO_OFF(USR_LED1);

		if (shared_mem->shepherd_mode == MODE_VIRTCAP)
			virtcap_init((struct VirtCapSettings *)&shared_mem
					     ->virtcap_settings,
				     (struct CalibrationSettings *)&shared_mem
					     ->calibration_settings);

		init_ring(&free_buffers);
		sampling_init((enum ShepherdMode)shared_mem->shepherd_mode,
			      shared_mem->harvesting_voltage);
		shared_mem->gpio_edges = NULL;

		/* Clear all interrupt events */
		CT_INTC.SICR_bit.STS_CLR_IDX = PRU_PRU_EVT_SAMPLE;
		CT_INTC.SICR_bit.STS_CLR_IDX = PRU_PRU_EVT_BLOCK_END;

		shared_mem->shepherd_state = STATE_IDLE;

		event_loop(shared_mem, &free_buffers, buffers);
	}
}
