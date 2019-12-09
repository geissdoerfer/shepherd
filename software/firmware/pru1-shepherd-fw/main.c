#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_iep.h>

#include "rpmsg.h"
#include "iep.h"
#include "gpio.h"
#include "intc.h"

#include "commons.h"
#include "shepherd_config.h"

/* The Arm to Host interrupt for the timestamp event is mapped to Host interrupt 0 -> Bit 30 (see resource_table.h) */
#define HOST_INT_TIMESTAMP (1U << 30)

#define DEBUG_P0 P8_41
#define DEBUG_P1 P8_42

/* The IEP is clocked with 200 MHz -> 5 nanoseconds per tick */
#define TIMER_TICK_NS 5
#define TIMER_BASE_PERIOD BUFFER_PERIOD_NS / TIMER_TICK_NS
#define SAMPLE_INTERVAL_NS BUFFER_PERIOD_NS / SAMPLES_PER_BUFFER

volatile struct SharedMem *shared_mem =
	(volatile struct SharedMem *)PRU_SHARED_MEM_STRUCT_OFFSET;

enum SyncState {
	IDLE,
	WAIT_IEP_WRAP,
	WAIT_HOST_INT,
	REQUEST_PENDING,
	REPLY_PENDING
};

int fault_handler(char *err_msg)
{
	/* If shepherd is not running, we can recover from the fault */
	if (shared_mem->shepherd_state != STATE_RUNNING) {
		printf(err_msg);
		return 0;
	}

	while (true) {
		printf(err_msg);
		__delay_cycles(2000000000);
	}
}

static inline int check_control_reply(struct CtrlRepMsg *ctrl_rep)
{
	int n;
	n = rpmsg_get((void *)ctrl_rep);

	if (n == sizeof(struct CtrlRepMsg)) {
		if (ctrl_rep->identifier != MSG_SYNC_CTRL_REP)
			return fault_handler("Wrong RPMSG ID");

		if (rpmsg_get((void *)ctrl_rep) > 0)
			return fault_handler("Extra pending messages");
		return 0;
	}
	return -1;
}

/*
 * Here, we sample the 4 GPIO pins from a connected sensor node. We repeatedly
 * poll the state via the R31 register and keep the last state in a static
 * variable. Once we detect a change, the new value (4bit) is written to the
 * corresponding buffer (which is managed by PRU0). The tricky part is the
 * synchronization between the PRUs to avoid inconsistent state, while
 * minimizing sampling delay
 */
static inline int check_gpio(volatile struct SharedMem *shared_mem,
			     uint64_t current_timestamp_ns,
			     unsigned int sample_counter,
			     unsigned int last_sample_ticks)
{
	static uint32_t prev_gpio_status = 0x00;

	/*
     * Only continue if shepherd is running and PRU0 acutally provides a buffer
     * to write to.
     */
	if ((shared_mem->shepherd_state != STATE_RUNNING) ||
	    (shared_mem->gpio_edges == NULL)) {
		prev_gpio_status = 0x00;
		return 0;
	}

	uint32_t now_status = __R31 & 0x0F;
	uint32_t diff = now_status ^ prev_gpio_status;

	prev_gpio_status = now_status;

	/* Each buffer can only store a limited number of events */
	if (shared_mem->gpio_edges->idx >= MAX_GPIO_EVT_PER_BUFFER)
		return -1;

	if (diff > 0) {
		/* Ticks since we've taken the last sample */
		uint32_t ticks_since_last_sample =
			CT_IEP.TMR_CNT - last_sample_ticks;

		/* Nanoseconds from current buffer start to last sample */
		uint32_t last_sample_ns = SAMPLE_INTERVAL_NS * sample_counter;

		/* Calculate final timestamp of gpio event */
		uint64_t gpio_timestamp =
			current_timestamp_ns + last_sample_ns +
			TIMER_TICK_NS * ticks_since_last_sample;

		simple_mutex_enter(&shared_mem->gpio_edges_mutex);
		shared_mem->gpio_edges
			->timestamp_ns[shared_mem->gpio_edges->idx] =
			gpio_timestamp;
		shared_mem->gpio_edges->bitmask[shared_mem->gpio_edges->idx] =
			(char)now_status;
		shared_mem->gpio_edges->idx += 1;
		simple_mutex_exit(&shared_mem->gpio_edges_mutex);
	}
	return 0;
}

/*
 * The firmware for synchronization/sample timing is based on a simple
 * event loop. There are three events: 1) Interrupt from Linux kernel module
 * 2) Local IEP timer wrapped 3) Local IEP timer compare for sampling
 *
 * Event 1:
 * The kernel module periodically timestamps its own clock and immediately
 * triggers an interrupt to PRU1. On reception of that interrupt we have
 * to timestamp our local IEP clock. We then send the local timestamp to the
 * kernel module as an RPMSG message. The kernel module runs a PI control loop
 * that minimizes the phase shift (and frequency deviation) by calculating a
 * correction factor that we apply to the base period of the IEP clock. This
 * resembles a Phase-Locked-Loop system. The kernel module sends the resulting
 * correction factor to us as an RPMSG. Ideally, Event 1 happens at the same
 * time as Event 2, i.e. our local clock should wrap at exactly the same time
 * as the Linux host clock. However, due to phase shifts and kernel timer
 * jitter, the two events typically happen with a small delay and in arbitrary
 * order. However, we would
 *
 * Event 2:
 *
 * Event 3:
 * This is the main sample trigger that is used to trigger the actual sampling
 * on PRU0 by raising an interrupt. After every sample, we have to forward
 * the compare value, taking into account the current sampling period
 * (dynamically adapted by PLL). Also, we will only check for the controller
 * reply directly following this event in order to avoid sampling jitter that
 * could result from being busy with RPMSG and delaying response to the next
 * Event 3
 */

int event_loop(volatile struct SharedMem *shared_mem)
{
	unsigned int sample_counter;
	uint64_t current_timestamp_ns;
	uint32_t last_sample_ticks;
	/*
     * Buffer for storing the control reply from Linux kernel module
     * needs to be large enough to hold the largest possible RPMSG
     */
	char rpmsg_buffer[256];
	struct CtrlRepMsg *ctrl_rep = (struct CtrlRepMsg *)&rpmsg_buffer;

	/* Prepare message that will be sent to Linux kernel module */
	struct CtrlReqMsg ctrl_req = { .identifier = MSG_SYNC_CTRL_REQ };

	/* This tracks our local state, allowing to execute actions at the right time */
	enum SyncState sync_state = IDLE;

	/*
     * This holds the number of 'compensation' periods, where the sampling
     * period is increased by 1 in order to compensate for the remainder of the
     * integer division used to calculate the sampling period
     */
	unsigned int n_comp = 0;

	/* Our initial guess of the sampling period based on nominal timer period */
	unsigned int sample_period = TIMER_BASE_PERIOD / SAMPLES_PER_BUFFER;

	/* These are our initial guesses for buffer sample period */
	iep_set_cmp_val(IEP_CMP0, TIMER_BASE_PERIOD);
	iep_set_cmp_val(IEP_CMP1, sample_period);

	iep_enable_evt_cmp(IEP_CMP1);
	iep_clear_evt_cmp(IEP_CMP0);

	/* Clear raw interrupt status from ARM host */
	INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);
	/* Wait for first timer interrupt from Linux host */
	while (!(__R31 & (1U << 30)))
		;
	if (INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP))
		INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

	iep_start();

	while (1) {
		check_gpio(shared_mem, current_timestamp_ns, sample_counter,
			   last_sample_ticks);

		/* Check for timer interrupt from Linux host [Event1] */
		if (__R31 & HOST_INT_TIMESTAMP) {
			if (!INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP))
				continue;

			/* Take timestamp of IEP */
			ctrl_req.ticks_iep = CT_IEP.TMR_CNT;

			/* Clear interrupt */
			INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

			/* Prepare and send control request to Linux host */
			ctrl_req.old_period = CT_IEP.TMR_CMP0;

			if (sync_state == WAIT_HOST_INT)
				sync_state = REQUEST_PENDING;
			else if (sync_state == IDLE)
				sync_state = WAIT_IEP_WRAP;
			else
				return fault_handler(
					"Wrong state at host interrupt");
		}
		/* Timer compare 0 handle [Event 2] */
		if (iep_check_evt_cmp(IEP_CMP0) == 0) {
			/* Tell PRU0 to take the first sample of this block */
			INTC_TRIGGER_EVENT(PRU_PRU_EVT_SAMPLE);
			/* Clear Timer Compare 0 */
			iep_clear_evt_cmp(IEP_CMP0);

			_GPIO_ON(DEBUG_P1);
			_GPIO_ON(DEBUG_P0);

			/* Reset sample counter and sample timer period */
			sample_counter = 1;
			iep_set_cmp_val(IEP_CMP1, sample_period);
			iep_enable_evt_cmp(IEP_CMP1);

			last_sample_ticks = 0;
			if (sync_state == WAIT_IEP_WRAP)
				sync_state = REQUEST_PENDING;
			else if (sync_state == IDLE)
				sync_state = WAIT_HOST_INT;
			else
				return fault_handler(
					"Wrong state at timer wrap");

			/* With wrap, we'll use next timestamp as base for GPIO timestamps */
			current_timestamp_ns = shared_mem->next_timestamp_ns;

			_GPIO_OFF(DEBUG_P0);
		}
		/* Timer compare 1 handle [Event 3] */
		if (iep_check_evt_cmp(IEP_CMP1) == 0) {
			if (++sample_counter == SAMPLES_PER_BUFFER) {
				/* Tell PRU0 to take the last sample in this block */
				INTC_TRIGGER_EVENT(PRU_PRU_EVT_BLOCK_END);
			} else {
				/* Tell PRU0 to take a sample */
				INTC_TRIGGER_EVENT(PRU_PRU_EVT_SAMPLE);
			}
			iep_clear_evt_cmp(IEP_CMP1);

			_GPIO_ON(DEBUG_P0);

			last_sample_ticks = iep_get_cmp_val(IEP_CMP1);
			if (sample_counter < SAMPLES_PER_BUFFER) {
				/* Forward sample timer based on current sample_period*/
				unsigned int next_cmp_val =
					last_sample_ticks + sample_period;
				/* If we are in compensation phase add one */
				if (n_comp > 0) {
					next_cmp_val += 1;
					n_comp--;
				}
				iep_set_cmp_val(IEP_CMP1, next_cmp_val);
			}

			if (sample_counter == SAMPLES_PER_BUFFER / 2) {
				_GPIO_OFF(DEBUG_P1);
			}

			/* If we are waiting for a reply from Linux kernel module */
			if (sync_state == REPLY_PENDING) {
				if (check_control_reply(ctrl_rep) == 0) {
					/* The new timer period is the base period plus the correction calculated by the controller */
					unsigned int block_period =
						TIMER_BASE_PERIOD +
						ctrl_rep->clock_corr;
					sample_period = (block_period -
							 CT_IEP.TMR_CMP1) /
							(SAMPLES_PER_BUFFER -
							 sample_counter);
					n_comp = (block_period -
						  CT_IEP.TMR_CMP1) %
						 (SAMPLES_PER_BUFFER -
						  sample_counter);
					CT_IEP.TMR_CMP0 = block_period;
					sync_state = IDLE;
					shared_mem->next_timestamp_ns =
						ctrl_rep->next_timestamp_ns;
				}
			} else if (sync_state == REQUEST_PENDING) {
				rpmsg_putraw(&ctrl_req,
					     sizeof(struct CtrlReqMsg));

				sync_state = REPLY_PENDING;
			}
			_GPIO_OFF(DEBUG_P0);
		}
	}
}
void main(void)
{
	/* Allow OCP master port access by the PRU so the PRU can read external memories */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

	_GPIO_OFF(DEBUG_P0);
	_GPIO_OFF(DEBUG_P1);

	rpmsg_init("rpmsg-shprd");
	__delay_cycles(1000);

	/* Enable 'timestamp' interrupt from ARM host */
	CT_INTC.EISR_bit.EN_SET_IDX = HOST_PRU_EVT_TIMESTAMP;

reset:
	printf("starting synch routine..");
	/* Make sure the mutex is clear */
	simple_mutex_exit(&shared_mem->gpio_edges_mutex);

	iep_init();
	iep_reset();

	event_loop(shared_mem);
	goto reset;
}
