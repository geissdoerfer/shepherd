#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/math64.h>

#include "sync_ctrl.h"
#include "pru_comm.h"

static uint32_t sys_ts_over_timer_wrap_ns = 0;
static uint64_t next_timestamp_ns = 0;
static uint64_t prev_timestamp_ns = 0; 	/* for plausibility-check */

void reset_prev_timestamp(void)
{
    prev_timestamp_ns = 0;
}

static enum hrtimer_restart trigger_loop_callback(struct hrtimer *timer_for_restart);
static enum hrtimer_restart sync_loop_callback(struct hrtimer *timer_for_restart);
static uint32_t trigger_loop_period_ns = 100000000; /* just initial value to avoid div0 */
/*
* add pre-trigger, because design previously aimed directly for busy pru_timer_wrap
* (50% chance that pru takes a less meaningful counter-reading after wrap)
* 1 ms + 5 us, this should be enough time for the ping-pong-messaging to complete before timer_wrap
*/
static const uint32_t ns_pre_trigger = 1005000;

/* Timer to trigger fast sync_loop */
struct hrtimer trigger_loop_timer;
struct hrtimer sync_loop_timer;

/* series of halving sleep cycles, sleep less coming slowly near a total of 100ms of sleep */
const static unsigned int timer_steps_ns[] = {
        20000000u, 20000000u,
        20000000u, 20000000u, 10000000u,
        5000000u,  2000000u,  1000000u,
        500000u,   200000u,   100000u,
        50000u,    20000u};
const static size_t timer_steps_ns_size = sizeof(timer_steps_ns) / sizeof(timer_steps_ns[0]);
//static unsigned int step_pos = 0;

// Sync-Routine - TODO: take these from pru-sharedmem
#define BUFFER_PERIOD_NS    	(100000000U) // TODO: there is already: trigger_loop_period_ns
#define ADC_SAMPLES_PER_BUFFER  (10000U)
#define TIMER_TICK_NS           (5U)
#define TIMER_BASE_PERIOD   	(BUFFER_PERIOD_NS / TIMER_TICK_NS)
#define SAMPLE_INTERVAL_NS  	(BUFFER_PERIOD_NS / ADC_SAMPLES_PER_BUFFER)
#define SAMPLE_PERIOD  	        (TIMER_BASE_PERIOD / ADC_SAMPLES_PER_BUFFER)
static uint32_t info_count = 6666; /* >6k triggers explanation-message once */
struct sync_data_s *sync_data;

int sync_exit(void)
{
	hrtimer_cancel(&trigger_loop_timer);
	hrtimer_cancel(&sync_loop_timer);
	kfree(sync_data);

	return 0;
}

int sync_init(uint32_t timer_period_ns)
{
	struct timespec ts_now;
	uint64_t now_ns_system;
	uint32_t ns_over_wrap;
	uint64_t ns_now_until_trigger;

	sync_data = kmalloc(sizeof(struct sync_data_s), GFP_KERNEL);
	if (!sync_data)
		return -1;
	sync_reset();

    /* Timestamp system clock */
    getnstimeofday(&ts_now);
    now_ns_system = (uint64_t)timespec_to_ns(&ts_now);

	/* timer for trigger, TODO: this needs better naming, make clear what it does */
	trigger_loop_period_ns = timer_period_ns; /* 100 ms */

	hrtimer_init(&trigger_loop_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
    trigger_loop_timer.function = &trigger_loop_callback;

    /* timer for Sync-Loop */
    hrtimer_init(&sync_loop_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
    sync_loop_timer.function = &sync_loop_callback;

	div_u64_rem(now_ns_system, timer_period_ns, &ns_over_wrap);
	if (ns_over_wrap > (timer_period_ns / 2))
    {
        /* target timer-wrap one ahead */
	    ns_now_until_trigger = 2 * timer_period_ns - ns_over_wrap - ns_pre_trigger;
    }
	else
    {
        /* target next timer-wrap */
	    ns_now_until_trigger = timer_period_ns - ns_over_wrap - ns_pre_trigger;
    }


	hrtimer_start(&trigger_loop_timer,
		      ns_to_ktime(now_ns_system + ns_now_until_trigger),
		      HRTIMER_MODE_ABS);

    hrtimer_start(&sync_loop_timer,
            ns_to_ktime(now_ns_system + 1000000),
            HRTIMER_MODE_ABS);

	return 0;
}

int sync_reset(void)
{
    sync_data->error_now = 0;
    sync_data->error_pre = 0;
    sync_data->error_dif = 0;
    sync_data->error_sum = 0;
	sync_data->clock_corr = 0;
    sync_data->previous_period = TIMER_BASE_PERIOD;
	return 0;
}

enum hrtimer_restart trigger_loop_callback(struct hrtimer *timer_for_restart)
{
	struct timespec ts_now;
	uint64_t ts_now_system_ns;
	uint64_t ns_now_until_trigger;

	/* Raise Interrupt on PRU, telling it to timestamp IEP */
	pru_comm_trigger(HOST_PRU_EVT_TIMESTAMP);

	/* Timestamp system clock */
	getnstimeofday(&ts_now);
	ts_now_system_ns = (uint64_t)timespec_to_ns(&ts_now);

	/*
     * Get distance of system clock from timer wrap.
     * Is negative, when interrupt happened before wrap, positive when after
     */
    div_u64_rem(ts_now_system_ns, trigger_loop_period_ns, &sys_ts_over_timer_wrap_ns);
    next_timestamp_ns = ts_now_system_ns + trigger_loop_period_ns - sys_ts_over_timer_wrap_ns;

    if (sys_ts_over_timer_wrap_ns > (trigger_loop_period_ns / 2))
    {
        /* normal use case (with pre-trigger) */
        /* self regulating formula that results in ~ trigger_loop_period_ns */
        ns_now_until_trigger = 2 * trigger_loop_period_ns - sys_ts_over_timer_wrap_ns - ns_pre_trigger;
    } else
    {
        printk(KERN_ERR "shprd.k: module missed a sync-trigger! -> last timestamp is now probably used twice by PRU\n");
        ns_now_until_trigger = trigger_loop_period_ns - sys_ts_over_timer_wrap_ns - ns_pre_trigger;
        sys_ts_over_timer_wrap_ns = 0u; /* invalidate this measurement */
	}
    // TODO: minor optimization
    //  - write next_timestamp_ns directly into shared mem, as well as the other values in sync_loop
    //  - the reply-message is not needed anymore (current pru-code has nothing to calculate beforehand and would just use prev values if no new message arrives

	hrtimer_forward(timer_for_restart, timespec_to_ktime(ts_now),
			ns_to_ktime(ns_now_until_trigger));

	return HRTIMER_RESTART;
}

/* Handler for ctrl-requests from PRU1 */
enum hrtimer_restart sync_loop_callback(struct hrtimer *timer_for_restart)
{
    struct CtrlReqMsg ctrl_req;
    struct CtrlRepMsg ctrl_rep;
    struct timespec ts_now;
    uint64_t ts_now_system_ns;
    static uint64_t ts_last_error_ns = 0;
    static const uint64_t quiet_time_ns = 10000000000; // 10 s
    static unsigned int step_pos = 0;
    /* Timestamp system clock */
    getnstimeofday(&ts_now);
    ts_now_system_ns = (uint64_t)timespec_to_ns(&ts_now);

    if (pru_comm_get_ctrl_request(&ctrl_req))
    {
        if (ctrl_req.identifier != MSG_SYNC_CTRL_REQ)
        {
            /* Error occurs if something writes over boundaries */
            printk(KERN_ERR "shprd.k: Kernel Recv_CtrlRequest -> mem corruption?\n");
        }

        sync_loop(&ctrl_rep, &ctrl_req);

        if (!pru_comm_send_ctrl_reply(&ctrl_rep))
        {
            /* Error occurs if PRU was not able to handle previous message in time */
            printk(KERN_WARNING "shprd.k: Kernel Send_CtrlResponse -> back-pressure\n");
        }

        /* resetting to longest sleep period */
        step_pos = 0;
    }
    else if ((ts_last_error_ns + quiet_time_ns < ts_now_system_ns) &&
            (prev_timestamp_ns + 2*trigger_loop_period_ns < ts_now_system_ns) &&
            (prev_timestamp_ns > 0))
    {
        ts_last_error_ns = ts_now_system_ns;
        printk(KERN_ERR "shprd.k: Faulty behaviour - PRU did not answer to trigger-request in time! \n");
    }

    hrtimer_forward(timer_for_restart, timespec_to_ktime(ts_now),
            ns_to_ktime(timer_steps_ns[step_pos])); /* variable sleep cycle */

    if (step_pos < timer_steps_ns_size - 1) step_pos++;

    return HRTIMER_RESTART;
}


int sync_loop(struct CtrlRepMsg *const ctrl_rep, const struct CtrlReqMsg *const ctrl_req)
{
	uint32_t iep_ts_over_timer_wrap_ns;
	uint64_t ns_per_tick_n30;   /* n30 means a fixed point shift left by 30 bits */

	/*
     * Based on the previous IEP timer period and the nominal timer period
     * we can estimate the real nanoseconds passing per tick
     * We operate on fixed point arithmetics by shifting by 30 bit
     */
    ns_per_tick_n30 = div_u64(((uint64_t)trigger_loop_period_ns << 30u), sync_data->previous_period);

	/* Get distance of IEP clock at interrupt from last timer wrap */
	if (sys_ts_over_timer_wrap_ns > 0u)
    {
        iep_ts_over_timer_wrap_ns = (uint32_t)((((uint64_t)ctrl_req->ticks_iep) * ns_per_tick_n30)>>30u);
    }
	else
    {
        /* (also) invalidate this measurement */
	    iep_ts_over_timer_wrap_ns = 0u;
    }

	/* Difference between system clock and IEP clock phase */
    sync_data->error_pre = sync_data->error_now; // TODO: new D (of PID) is not in sysfs yet
    sync_data->error_now = (int64_t)iep_ts_over_timer_wrap_ns - (int64_t)sys_ts_over_timer_wrap_ns;
    sync_data->error_dif = sync_data->error_now - sync_data->error_pre;
    if (sync_data->error_now < - (int64_t)(trigger_loop_period_ns / 2))
    {
        /* Currently the correction is (almost) always headed in one direction
         * - the pre-trigger @ - 1 ms is the "almost" (1 % chance for the other direction)
         * - lets correct the imbalance to 50/50
         */
        sync_data->error_now += trigger_loop_period_ns;
    }
    sync_data->error_sum += sync_data->error_now; // integral should be behind controller, because current P-value is twice in calculation

	/* This is the actual PI controller equation
	 * NOTE1: unit of clock_corr in pru is ticks, but input is based on nanosec
     * NOTE2: traces show, that quantization noise could be a problem. example: K-value of 127, divided by 128 will still be 0, ringing is around ~ +-150
     * previous parameters were:    P=1/32, I=1/128, correction settled at ~1340 with values from 1321 to 1359
     * current parameters:          P=1/100,I=1/300, correction settled at ~1332 with values from 1330 to 1335
     * */
    sync_data->clock_corr = (int32_t)(div_s64(sync_data->error_now, 128) + div_s64(sync_data->error_sum, 256));
    if (sync_data->clock_corr > +80000) sync_data->clock_corr = +80000; /* 0.4 % -> ~12s for phase lock */
    if (sync_data->clock_corr < -80000) sync_data->clock_corr = -80000;

    /* determine corrected loop_ticks for next buffer_block */
    ctrl_rep->buffer_block_period = TIMER_BASE_PERIOD + sync_data->clock_corr;
    sync_data->previous_period = ctrl_rep->buffer_block_period;
    ctrl_rep->analog_sample_period = (ctrl_rep->buffer_block_period / ADC_SAMPLES_PER_BUFFER);
    ctrl_rep->compensation_steps = ctrl_rep->buffer_block_period - (ADC_SAMPLES_PER_BUFFER * ctrl_rep->analog_sample_period);

    if (((sync_data->error_now > 500) || (sync_data->error_now < -500)) && (++info_count >= 100)) /* val = 200 prints every 20s when enabled */
    {
        printk(KERN_INFO "shprd.sync: period=%u, n_comp=%u, er_pid=%lld/%lld/%lld, ns_iep=%u, ns_sys=%u\n",
                ctrl_rep->analog_sample_period, // = upper part of buffer_block_period
                ctrl_rep->compensation_steps,  // = lower part of buffer_block_period
                sync_data->error_now,
                sync_data->error_sum,
                sync_data->error_dif,
                iep_ts_over_timer_wrap_ns,
                sys_ts_over_timer_wrap_ns);
        if (info_count > 6600)
            printk(KERN_INFO "shprd.sync: NOTE - previous message is shown every 10 s when sync-error exceeds a threshold (ONLY normal during startup)\n");
        info_count = 0;
    }

	/* plausibility-check, in case the sync-algo produces jumps */
	if (prev_timestamp_ns > 0)
    {
        int64_t diff_timestamp_ms = div_s64((int64_t)next_timestamp_ns - prev_timestamp_ns, 1000000u);
        if (diff_timestamp_ms < 0)
            printk(KERN_ERR "shprd.k: backwards timestamp-jump detected (sync-loop, %lld ms)\n", diff_timestamp_ms);
        else if (diff_timestamp_ms < 95)
            printk(KERN_ERR "shprd.k: too small timestamp-jump detected (sync-loop, %lld ms)\n", diff_timestamp_ms);
        else if (diff_timestamp_ms > 105)
            printk(KERN_ERR "shprd.k: forwards timestamp-jump detected (sync-loop, %lld ms)\n", diff_timestamp_ms);
        else if (next_timestamp_ns == 0)
            printk(KERN_ERR "shprd.k: zero timestamp detected (sync-loop)\n");
    }
    prev_timestamp_ns = next_timestamp_ns;
    ctrl_rep->next_timestamp_ns = next_timestamp_ns;

    if ((ctrl_rep->buffer_block_period > TIMER_BASE_PERIOD + 80000) || (ctrl_rep->buffer_block_period < TIMER_BASE_PERIOD - 80000))
        printk(KERN_ERR "shprd.k: buffer_block_period out of limits (%u instead of ~20M)", ctrl_rep->buffer_block_period);
    if ((ctrl_rep->analog_sample_period > SAMPLE_PERIOD + 10) || (ctrl_rep->analog_sample_period < SAMPLE_PERIOD - 10))
        printk(KERN_ERR "shprd.k: analog_sample_period out of limits (%u instead of ~2000)", ctrl_rep->analog_sample_period);

    return 0;
}