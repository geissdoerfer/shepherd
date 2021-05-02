#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_iep.h>
#include <gpio.h>

#include "rpmsg.h"
#include "iep.h"
#include "intc.h"

#include "commons.h"
#include "shepherd_config.h"
#include "stdint_fast.h"
#include "debug_routines.h"

/* The Arm to Host interrupt for the timestamp event is mapped to Host interrupt 0 -> Bit 30 (see resource_table.h) */
#define HOST_INT_TIMESTAMP_MASK (1U << 30U)
#define PRU_INT_MASK 		(1U << 31U)

#define DEBUG_PIN0_MASK         BIT_SHIFT(P8_41)
#define DEBUG_PIN1_MASK         BIT_SHIFT(P8_42)

#define GPIO_MASK		(0x0F)

#define SANITY_CHECKS		(0)	// warning: costs performance, but is helpful for dev / debugging

enum SyncState {
	IDLE,
	REPLY_PENDING
};

//static void fault_handler(const uint32_t shepherd_state, const char * err_msg) // TODO: use when pssp gets changed
static void fault_handler(const uint32_t shepherd_state, char * err_msg)
{
	/* If shepherd is not running, we can recover from the fault */
	if (shepherd_state != STATE_RUNNING)
	{
		printf(err_msg);
		return;
	}

	while (true)
	{
		printf(err_msg);
		__delay_cycles(2000000000U);
	}
}


static inline bool_ft receive_control_reply(volatile struct SharedMem *const shared_mem, struct CtrlRepMsg *const ctrl_rep)
{
	if (shared_mem->ctrl_rep.msg_unread >= 1)
	{
		if (shared_mem->ctrl_rep.identifier != MSG_SYNC_CTRL_REP)
		{
			/* Error occurs if something writes over boundaries */
			fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> mem corruption?");
		}
		*ctrl_rep = shared_mem->ctrl_rep; // TODO: faster to copy only the needed 2 uint32
		shared_mem->ctrl_rep.msg_unread = 0;

		if (SANITY_CHECKS)
		{
			/* sanity-check of vars */
			static uint64_t prev_timestamp_ns = 0;
			if ((ctrl_rep->buffer_block_period > TIMER_BASE_PERIOD + 80000) || (ctrl_rep->buffer_block_period < TIMER_BASE_PERIOD - 80000))
				fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> buffer_block_period out of limits");
			if ((ctrl_rep->analog_sample_period > SAMPLE_PERIOD + 10) || (ctrl_rep->buffer_block_period < SAMPLE_PERIOD - 10))
				fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> analog_sample_period out of limits");
			const uint64_t time_diff = ctrl_rep->next_timestamp_ns - prev_timestamp_ns;
			if ((time_diff != BUFFER_PERIOD_NS) && (prev_timestamp_ns > 0)) {
				if (ctrl_rep->next_timestamp_ns == 0)
					fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> next_timestamp_ns is zero");
				else if (time_diff > BUFFER_PERIOD_NS + 5000000)
					fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> next_timestamp_ns is > 105 ms");
				else if (time_diff < BUFFER_PERIOD_NS - 5000000)
					fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> next_timestamp_ns is < 95 ms");
				else
					fault_handler(shared_mem->shepherd_state, "Recv_CtrlReply -> timestamp-jump was not 100 ms");
			}
			prev_timestamp_ns = ctrl_rep->next_timestamp_ns;
		}
		return 1;
	}
	return 0;
}

// send emits a 1 on success
// ctrl_req: (future opt.) needs to have special config set: identifier=MSG_SYNC_CTRL_REQ and msg_unread=1
static inline bool_ft send_control_request(volatile struct SharedMem *const shared_mem, const struct CtrlReqMsg *const ctrl_req)
{
	if (shared_mem->ctrl_req.msg_unread == 0)
	{
		shared_mem->ctrl_req = *ctrl_req;
		shared_mem->ctrl_req.identifier = MSG_SYNC_CTRL_REQ; // TODO: is better done in request from argument
		// NOTE: always make sure that the unread-flag is activated AFTER payload is copied
		shared_mem->ctrl_req.msg_unread = 1u;
		return 1;
	}
	/* Error occurs if PRU was not able to handle previous message in time */
	fault_handler(shared_mem->shepherd_state, "Send_CtrlReq -> back-pressure");
	return 0;
}

/*
 * Here, we sample the the GPIO pins from a connected sensor node. We repeatedly
 * poll the state via the R31 register and keep the last state in a static
 * variable. Once we detect a change, the new value (V1=4bit, V2=10bit) is written to the
 * corresponding buffer (which is managed by PRU0). The tricky part is the
 * synchronization between the PRUs to avoid inconsistent state, while
 * minimizing sampling delay
 */
static inline void check_gpio(volatile struct SharedMem *const shared_mem, const uint32_t last_sample_ticks)
{
	static uint32_t prev_gpio_status = 0x00;

	/*
	* Only continue if shepherd is running and PRU0 actually provides a buffer
	* to write to.
	*/
	if ((shared_mem->shepherd_state != STATE_RUNNING) ||
	    (shared_mem->gpio_edges == NULL)) {
		prev_gpio_status = 0x00;
		return;
	}

	const uint32_t gpio_status = read_r31() & GPIO_MASK;
	const uint32_t gpio_diff = gpio_status ^ prev_gpio_status;

	prev_gpio_status = gpio_status;

	if (gpio_diff > 0)
	{
		DEBUG_GPIO_STATE_2;
		// local copy reduces reads to far-ram to current minimum
		const uint32_t cIDX = shared_mem->gpio_edges->idx;

		/* Each buffer can only store a limited number of events */
		if (cIDX >= MAX_GPIO_EVT_PER_BUFFER) return;

		/* Ticks since we've taken the last sample */
		const uint32_t ticks_since_last_sample = CT_IEP.TMR_CNT - last_sample_ticks;
		/* Calculate final timestamp of gpio event */
		const uint64_t gpio_timestamp_ns = shared_mem->last_sample_timestamp_ns + TIMER_TICK_NS * ticks_since_last_sample;

		simple_mutex_enter(&shared_mem->gpio_edges_mutex);
		shared_mem->gpio_edges->timestamp_ns[cIDX] = gpio_timestamp_ns;
		shared_mem->gpio_edges->bitmask[cIDX] = (uint8_t)gpio_status;
		shared_mem->gpio_edges->idx = cIDX + 1;
		simple_mutex_exit(&shared_mem->gpio_edges_mutex);
	}
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

int32_t event_loop(volatile struct SharedMem *const shared_mem)
{
	uint32_t last_analog_sample_ticks = 0;

	/* Prepare message that will be received and sent to Linux kernel module */
	struct CtrlReqMsg ctrl_req = { .identifier = MSG_SYNC_CTRL_REQ, .msg_unread = 1 };
	struct CtrlRepMsg ctrl_rep = {
		.buffer_block_period = TIMER_BASE_PERIOD,
		.analog_sample_period = TIMER_BASE_PERIOD / ADC_SAMPLES_PER_BUFFER,
		.compensation_steps = 0u,
	};

	/* This tracks our local state, allowing to execute actions at the right time */
	enum SyncState sync_state = IDLE;

	/*
	* This holds the number of 'compensation' periods, where the sampling
	* period is increased by 1 in order to compensate for the remainder of the
	* integer division used to calculate the sampling period.
	*/
	uint32_t compensation_steps = ctrl_rep.compensation_steps;
	/*
	 * holds distribution of the compensation periods (every x samples the period is increased by 1)
	 */
	uint32_t compensation_counter = 0u;
	uint32_t compensation_increment = 0u;

	/* Our initial guess of the sampling period based on nominal timer period */
	uint32_t analog_sample_period = ctrl_rep.analog_sample_period;
	uint32_t buffer_block_period = ctrl_rep.buffer_block_period;

	/* These are our initial guesses for buffer sample period */
	iep_set_cmp_val(IEP_CMP0, TIMER_BASE_PERIOD);  // 20 MTicks -> 100 ms
	iep_set_cmp_val(IEP_CMP1, buffer_block_period); // 20 kTicks -> 10 us

	iep_enable_evt_cmp(IEP_CMP1);
	iep_clear_evt_cmp(IEP_CMP0);

	/* Clear raw interrupt status from ARM host */
	INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);
	/* Wait for first timer interrupt from Linux host */
	while (!(read_r31() & HOST_INT_TIMESTAMP_MASK)) {};

	if (INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP)) INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

	iep_start();

	while (1)
	{

		#if DEBUG_LOOP_EN
		debug_loop_delays(shared_mem->shepherd_state);
		#endif

		DEBUG_GPIO_STATE_1;
		    check_gpio(shared_mem, last_analog_sample_ticks);
		DEBUG_GPIO_STATE_0;

		/* [Event1] Check for interrupt from Linux host to take timestamp */
		if (read_r31() & HOST_INT_TIMESTAMP_MASK)
		{
			if (!INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP)) continue;

			/* Take timestamp of IEP */
			ctrl_req.ticks_iep = iep_get_cnt_val();
			DEBUG_EVENT_STATE_2;
			/* Clear interrupt */
			INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

			if (sync_state == IDLE)    sync_state = REPLY_PENDING;
			else {
				fault_handler(shared_mem->shepherd_state,"Sync not idle at host interrupt");
				return 0;
			}
			send_control_request(shared_mem, &ctrl_req);
			DEBUG_EVENT_STATE_0;
			continue;  // for more regular gpio-sampling
		}

		/*  [Event 2] Timer compare 0 handle -> trigger for buffer swap on pru0 */
		if (shared_mem->cmp0_trigger_for_pru1)
		{
			DEBUG_EVENT_STATE_2;
			// reset trigger
			shared_mem->cmp0_trigger_for_pru1 = 0;

			/* update clock compensation of sample-trigger */
			iep_set_cmp_val(IEP_CMP1, 0);
			iep_enable_evt_cmp(IEP_CMP1);
			analog_sample_period = ctrl_rep.analog_sample_period;
			compensation_steps = ctrl_rep.compensation_steps;
			compensation_increment = ctrl_rep.compensation_steps;
			compensation_counter = 0;

			/* update main-loop */
			buffer_block_period = ctrl_rep.buffer_block_period;
			iep_set_cmp_val(IEP_CMP0, buffer_block_period);

			/* more maintenance */
			last_analog_sample_ticks = 0;

			DEBUG_EVENT_STATE_0;
			continue; // for more regular gpio-sampling
		}

		/* [Event 3] Timer compare 1 handle -> trigger for analog sample on pru0 */
		if (shared_mem->cmp1_trigger_for_pru1)
		{
			/* prevent a race condition (cmp0_event has to happen before cmp1_event!) */
			if (shared_mem->cmp0_trigger_for_pru1) continue;

			DEBUG_EVENT_STATE_1;
			// reset trigger
			shared_mem->cmp1_trigger_for_pru1 = 0;

			// Update Timer-Values
			last_analog_sample_ticks = iep_get_cmp_val(IEP_CMP1);
			if (last_analog_sample_ticks > 0) // this assumes sample0 taken on cmp1==0
			{
				shared_mem->last_sample_timestamp_ns += SAMPLE_INTERVAL_NS;
			}

			/* Forward sample timer based on current analog_sample_period*/
			uint32_t next_cmp_val = last_analog_sample_ticks + analog_sample_period;
			compensation_counter += compensation_increment; // fixed point magic
			/* If we are in compensation phase add one */
			if ((compensation_counter >= ADC_SAMPLES_PER_BUFFER) && (compensation_steps > 0)) {
				next_cmp_val += 1;
				compensation_steps--;
				compensation_counter -= ADC_SAMPLES_PER_BUFFER;
			}
			iep_set_cmp_val(IEP_CMP1, next_cmp_val);

			/* If we are waiting for a reply from Linux kernel module */
			if (receive_control_reply(shared_mem, &ctrl_rep) > 0)
			{
				sync_state = IDLE;
				shared_mem->next_buffer_timestamp_ns = ctrl_rep.next_timestamp_ns;
			}
			DEBUG_EVENT_STATE_0;
			continue; // for more regular gpio-sampling
		}
	}
}

void main(void)
{
	volatile struct SharedMem *const shared_mememory = (volatile struct SharedMem *)PRU_SHARED_MEM_STRUCT_OFFSET;

    	/* Allow OCP master port access by the PRU so the PRU can read external memories */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
	DEBUG_STATE_0;

	rpmsg_init("rpmsg-shprd");
	__delay_cycles(1000);

	/* Enable 'timestamp' interrupt from ARM host */
	CT_INTC.EISR_bit.EN_SET_IDX = HOST_PRU_EVT_TIMESTAMP;

reset:
	printf("(re)starting sync routine..");
	/* Make sure the mutex is clear */
	simple_mutex_exit(&shared_mememory->gpio_edges_mutex);

	iep_init();
	iep_reset();

	event_loop(shared_mememory);
	goto reset;
}
