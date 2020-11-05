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
#define HOST_INT_TIMESTAMP (1U << 30U)
#define PRU_INT 	(1U << 31U)

#define DEBUG_P0            BIT_SHIFT(P8_41)
#define DEBUG_P1            BIT_SHIFT(P8_42)

/* The IEP is clocked with 200 MHz -> 5 nanoseconds per tick */
#define TIMER_TICK_NS       5U
#define TIMER_BASE_PERIOD   (BUFFER_PERIOD_NS / TIMER_TICK_NS)
#define SAMPLE_INTERVAL_NS  (BUFFER_PERIOD_NS / ADC_SAMPLES_PER_BUFFER)

enum SyncState {
	IDLE,
	WAIT_IEP_WRAP,
	WAIT_HOST_INT,
	REQUEST_PENDING,
	REPLY_PENDING
};

//static void fault_handler(const uint32_t shepherd_state, const char * err_msg) // TODO: use when pssp gets changed
static void fault_handler(const uint32_t shepherd_state, char * err_msg)
{
	/* If shepherd is not running, we can recover from the fault */
	if (shepherd_state != STATE_RUNNING) {
		printf(err_msg);
		return;
	}

	while (true)
	{
	printf(err_msg);
	__delay_cycles(2000000000U);
	}
}



static inline bool_ft check_control_reply(const uint32_t shepherd_state, struct CtrlRepMsg *const ctrl_rep)
{
	const uint32_t n = rpmsg_get((void *)ctrl_rep);

	if (n == sizeof(struct CtrlRepMsg)) {
		if (ctrl_rep->identifier != MSG_SYNC_CTRL_REP)  fault_handler(shepherd_state, "Wrong RPMSG ID");
		else if (rpmsg_get((uint8_t *)ctrl_rep) > 0)    fault_handler(shepherd_state, "Extra pending messages");
		// TODO: why is an extra message so bad? most likely sign for out of sync [this rpmsg_get-check is quiet expensive]
		return 1;
	}
	return 0;
}

/*
 * Here, we sample the 4 GPIO pins from a connected sensor node. We repeatedly
 * poll the state via the R31 register and keep the last state in a static
 * variable. Once we detect a change, the new value (4bit) is written to the
 * corresponding buffer (which is managed by PRU0). The tricky part is the
 * synchronization between the PRUs to avoid inconsistent state, while
 * minimizing sampling delay
 */
static inline void check_gpio(volatile struct SharedMem *const shared_mem,
        const uint64_t current_timestamp_ns,
        const uint32_t last_sample_ticks)
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

	const uint32_t gpio_status = read_r31() & 0x0F;
	const uint32_t diff = gpio_status ^ prev_gpio_status;

	prev_gpio_status = gpio_status;

	if (diff > 0)
	{
		DEBUG_GPIO_STATE_2;
	    	// TODO: reading from ram/far is slow, idx should be stored locally, potentially unsafe,
		const uint32_t cIDX = shared_mem->gpio_edges->idx; // reduces reads to far-ram to current minimum
		/* Each buffer can only store a limited number of events */
		if (cIDX >= MAX_GPIO_EVT_PER_BUFFER) return;

		/* Ticks since we've taken the last sample */
		const uint32_t ticks_since_last_sample = CT_IEP.TMR_CNT - last_sample_ticks;

		/* Nanoseconds from current buffer start to last sample */
		const uint32_t last_sample_ns = SAMPLE_INTERVAL_NS * (shared_mem->analog_sample_counter);

		/* Calculate final timestamp of gpio event */
		const uint64_t gpio_timestamp = current_timestamp_ns + last_sample_ns + TIMER_TICK_NS * ticks_since_last_sample;

		simple_mutex_enter(&shared_mem->gpio_edges_mutex);
		shared_mem->gpio_edges->timestamp_ns[cIDX] = gpio_timestamp;
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
	uint64_t current_timestamp_ns = 0;
	uint32_t last_analog_sample_ticks = 0;
	/*
	* Buffer for storing the control reply from Linux kernel module
	* needs to be large enough to hold the largest possible RPMSG
	*/
	uint8_t rpmsg_buffer[256];
	struct CtrlRepMsg *const ctrl_rep = (struct CtrlRepMsg *)&rpmsg_buffer;

	/* Prepare message that will be sent to Linux kernel module */
	struct CtrlReqMsg ctrl_req = { .identifier = MSG_SYNC_CTRL_REQ };

	/* This tracks our local state, allowing to execute actions at the right time */
	enum SyncState sync_state = IDLE;

	/*
	* This holds the number of 'compensation' periods, where the sampling
	* period is increased by 1 in order to compensate for the remainder of the
	* integer division used to calculate the sampling period
	*/
	uint32_t n_comp = 0;

	/* Our initial guess of the sampling period based on nominal timer period */
	uint32_t analog_sample_period = TIMER_BASE_PERIOD / ADC_SAMPLES_PER_BUFFER;

	/* These are our initial guesses for buffer sample period */
	iep_set_cmp_val(IEP_CMP0, TIMER_BASE_PERIOD);  // 20 MTicks -> 100 ms
	iep_set_cmp_val(IEP_CMP1, analog_sample_period); // 20 kTicks -> 10 us

	iep_enable_evt_cmp(IEP_CMP1);
	iep_clear_evt_cmp(IEP_CMP0);

	/* Clear raw interrupt status from ARM host */
	INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);
	/* Wait for first timer interrupt from Linux host */
	while (!(read_r31() & HOST_INT_TIMESTAMP)) {};

	if (INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP)) INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

	iep_start();

	while (1)
	{

		#if DEBUG_LOOP_EN
		debug_loop_delays(shared_mem->shepherd_state);
		#endif

		DEBUG_GPIO_STATE_1;
		    check_gpio(shared_mem, current_timestamp_ns, last_analog_sample_ticks);
		DEBUG_GPIO_STATE_0;

		/* Check for timer interrupt from Linux host [Event1] */
		if (read_r31() & HOST_INT_TIMESTAMP) {
			if (!INTC_CHECK_EVENT(HOST_PRU_EVT_TIMESTAMP)) continue;

			/* Take timestamp of IEP */
			ctrl_req.ticks_iep = iep_get_cnt_val();
			DEBUG_EVENT_STATE_1;
			/* Clear interrupt */
			INTC_CLEAR_EVENT(HOST_PRU_EVT_TIMESTAMP);

			/* Prepare and send control request to Linux host */
			ctrl_req.old_period = iep_get_cmp_val(IEP_CMP0);

			if (sync_state == WAIT_HOST_INT)    sync_state = REQUEST_PENDING;
			else if (sync_state == IDLE)        sync_state = WAIT_IEP_WRAP;
			else {
				fault_handler(shared_mem->shepherd_state,"Wrong state at host interrupt");
				return 0;
			}
			DEBUG_EVENT_STATE_0;
		}

		// take a snapshot of current triggers -> ensures prioritized handling
		// edge case: sample0 @cnt=0, cmp0&1 trigger, but cmp0 needs to get handled before cmp1
		const uint32_t iep_tmr_cmp_sts = iep_get_tmr_cmp_sts();

		/* Timer compare 0 handle [Event 2] */
		if (iep_check_evt_cmp_fast(iep_tmr_cmp_sts, IEP_CMP0S))
		{
			DEBUG_EVENT_STATE_2;
			shared_mem->cmp0_handled_by_pru1 = 1;

			/* Clear Timer Compare 0 */
			iep_clear_evt_cmp(IEP_CMP0); // CT_IEP.TMR_CMP_STS.bit0

			/* Reset sample counter and sample timer period (if needed) */
			if (iep_get_cmp_val(IEP_CMP1) > iep_get_cmp_val(IEP_CMP0)) iep_set_cmp_val(IEP_CMP1, analog_sample_period);
   			iep_enable_evt_cmp(IEP_CMP1);

			last_analog_sample_ticks = 0;
			if (sync_state == WAIT_IEP_WRAP)    sync_state = REQUEST_PENDING;
			else if (sync_state == IDLE)        sync_state = WAIT_HOST_INT;
			else {
				fault_handler(shared_mem->shepherd_state, "Wrong state at timer wrap");
				return 0;
			}

			/* With wrap, we'll use next timestamp as base for GPIO timestamps */
			current_timestamp_ns = shared_mem->next_timestamp_ns;
			// TODO: shouldn't it always be the shared_mem timestamp? timestamping could be wrong after buffer exchange, but before this update

			DEBUG_EVENT_STATE_0;
		}

		/* Timer compare 1 handle [Event 3] */
		if (iep_check_evt_cmp_fast(iep_tmr_cmp_sts, IEP_CMP1S))
		{
			shared_mem->cmp1_handled_by_pru1 = 1;
			// Relict from previous INTC-System -> TODO: Remove if safe
			if (INTC_CHECK_EVENT(PRU_PRU_EVT_SAMPLE)) INTC_CLEAR_EVENT(PRU_PRU_EVT_SAMPLE);
			if (INTC_CHECK_EVENT(PRU_PRU_EVT_BLOCK_END)) INTC_CLEAR_EVENT(PRU_PRU_EVT_BLOCK_END);

			/* Important: We have to clear the interrupt here, to avoid missing interrupts */
			iep_clear_evt_cmp(IEP_CMP1);

			// Update Timer-Values
			last_analog_sample_ticks = iep_get_cmp_val(IEP_CMP1);
			/* Forward sample timer based on current sample_period*/
			uint32_t next_cmp_val = last_analog_sample_ticks + analog_sample_period;
			/* If we are in compensation phase add one */
			if (n_comp > 0) {
				next_cmp_val += 1;
				n_comp--;
			}
			// handle edge-case: check if next compare-value is behind auto-reset of cmp0
			const uint32_t timer_cmp0_value = iep_get_cmp_val(IEP_CMP0); // read costs 12 Cycles
			if (next_cmp_val > timer_cmp0_value) next_cmp_val -= timer_cmp0_value;
			iep_set_cmp_val(IEP_CMP1, next_cmp_val);


			/* If we are waiting for a reply from Linux kernel module */
			if (sync_state == REPLY_PENDING)
			{
				DEBUG_EVENT_STATE_3;
				if (check_control_reply(shared_mem->shepherd_state, ctrl_rep) > 0)
				{
					uint32_t block_period;
					/* The new timer period is the base period plus the correction calculated by the controller */
					if (ctrl_rep->clock_corr > (int32_t)(TIMER_BASE_PERIOD / 10))
						block_period = TIMER_BASE_PERIOD + TIMER_BASE_PERIOD / 10;
					else if (ctrl_rep->clock_corr < -(int32_t)(TIMER_BASE_PERIOD / 10))
						block_period = TIMER_BASE_PERIOD - TIMER_BASE_PERIOD / 10;
					else
						block_period = TIMER_BASE_PERIOD + ctrl_rep->clock_corr;

					// determine resulting new sample period, n_comp is the remainder of the division
					const uint32_t block_period_remain = (block_period - iep_get_cmp_val(IEP_CMP1));
					const uint32_t samples_remain = (ADC_SAMPLES_PER_BUFFER - (shared_mem->analog_sample_counter));
					analog_sample_period = block_period_remain / samples_remain;
					n_comp = block_period_remain - (analog_sample_period * samples_remain);

					iep_set_cmp_val(IEP_CMP0, block_period);
					sync_state = IDLE;
					shared_mem->next_timestamp_ns =  ctrl_rep->next_timestamp_ns;
				}
				DEBUG_EVENT_STATE_0;
			}
			else if (sync_state == REQUEST_PENDING)
			{
				// To stay consistent with timing throw away a possible pre-received Replies (that +1 can stay in the system forever)
				check_control_reply(shared_mem->shepherd_state, ctrl_rep);
				// send request
				rpmsg_putraw(&ctrl_req, sizeof(struct CtrlReqMsg));
				sync_state = REPLY_PENDING;
			}
		}

		if ((shared_mem->cmp0_handled_by_pru0 != 0) && (shared_mem->cmp0_handled_by_pru1 != 0))
		{
			shared_mem->cmp0_handled_by_pru0 = 0;
			shared_mem->cmp0_handled_by_pru1 = 0;
		}

		if ((shared_mem->cmp1_handled_by_pru0 != 0) && (shared_mem->cmp1_handled_by_pru1 != 0))
		{
			shared_mem->cmp1_handled_by_pru0 = 0;
			shared_mem->cmp1_handled_by_pru1 = 0;
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
	printf("starting synch routine..");
	/* Make sure the mutex is clear */
	simple_mutex_exit(&shared_mememory->gpio_edges_mutex);

	iep_init();
	iep_reset();

	event_loop(shared_mememory);
	goto reset;
}
