#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/math64.h>

#include "sync_ctrl.h"
#include "pru_comm.h"

struct sync_data_s *sync_data;

static int64_t ns_sys_to_wrap;
static uint64_t next_timestamp_ns;

static enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart);
static enum hrtimer_restart synch_loop_callback(struct hrtimer *timer_for_restart);

/* We wrap the timer to pass arguments to the callback function */
static struct hrtimer_wrap_s {
	uint32_t timer_period_ns;
	struct hrtimer hr_timer;
} trigger_timer;

/* Timer to trigger fast synch_loop */
struct hrtimer synch_loop_timer;
/* series of halving sleep cycles, sleep less coming slowly near a total of 100ms of sleep */
const static unsigned int timer_steps_ns[] = {
        20000000u, 20000000u,
        20000000u, 20000000u, 10000000u,
        5000000u,  2000000u,  1000000u,
        500000u,   200000u,   100000u,
        50000u,    20000u};
const static size_t timer_steps_ns_size = sizeof(timer_steps_ns) / sizeof(timer_steps_ns[0]);
//static unsigned int step_pos = 0;

int sync_exit(void)
{
	hrtimer_cancel(&trigger_timer.hr_timer);
	hrtimer_cancel(&synch_loop_timer);
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
	trigger_timer.timer_period_ns = timer_period_ns; /* 100 ms */

	hrtimer_init(&trigger_timer.hr_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	trigger_timer.hr_timer.function = &timer_callback;

    /* timer for Synch-Loop */
    hrtimer_init(&synch_loop_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
    synch_loop_timer.function = &synch_loop_callback;

	div_u64_rem(now_ns_system, timer_period_ns, &ns_over_wrap);
	if (ns_over_wrap > (timer_period_ns / 2))
		ns_now_until_trigger = 2 * timer_period_ns - ns_over_wrap;
	else
		ns_now_until_trigger = timer_period_ns - ns_over_wrap;

	hrtimer_start(&trigger_timer.hr_timer,
		      ns_to_ktime(now_ns_system + ns_now_until_trigger),
		      HRTIMER_MODE_ABS);

    hrtimer_start(&synch_loop_timer,
            ns_to_ktime(now_ns_system + 1000000),
            HRTIMER_MODE_ABS);

	return 0;
}

int sync_reset(void)
{
	sync_data->err_sum = 0;
	sync_data->err = 0;
	sync_data->clock_corr = 0;
	return 0;
}

enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart)
{
	struct timespec ts_now;
	uint64_t now_ns_system;
	uint32_t ns_over_wrap;
	uint64_t ns_now_until_trigger;

	struct hrtimer_wrap_s *wrapped_timer;

	wrapped_timer = container_of(timer_for_restart, struct hrtimer_wrap_s,
				     hr_timer);

	/* Raise Interrupt on PRU, telling it to timestamp IEP */
	pru_comm_trigger(HOST_PRU_EVT_TIMESTAMP);
	/* Timestamp system clock */
	getnstimeofday(&ts_now);

	now_ns_system = (uint64_t)timespec_to_ns(&ts_now);

	/*
     * Get distance of system clock from timer wrap.
     * Is negative, when interrupt happened before wrap, positive when after
     */
	div_u64_rem(now_ns_system, wrapped_timer->timer_period_ns,
		    &ns_over_wrap);
	if (ns_over_wrap > (wrapped_timer->timer_period_ns / 2)) {
		ns_sys_to_wrap =
			((int64_t)ns_over_wrap - wrapped_timer->timer_period_ns)
			<< 32;
		next_timestamp_ns = now_ns_system - ns_over_wrap +
				    2 * wrapped_timer->timer_period_ns;
		ns_now_until_trigger =
			2 * wrapped_timer->timer_period_ns - ns_over_wrap;
	} else {
		ns_sys_to_wrap = ((int64_t)ns_over_wrap) << 32;
		next_timestamp_ns = now_ns_system - ns_over_wrap +
				    wrapped_timer->timer_period_ns;
		ns_now_until_trigger =
			wrapped_timer->timer_period_ns - ns_over_wrap;
	}

	hrtimer_forward(timer_for_restart, timespec_to_ktime(ts_now),
			ns_to_ktime(ns_now_until_trigger));

	return HRTIMER_RESTART;
}

/* Handler for ctrl-requests from PRU1 */
enum hrtimer_restart synch_loop_callback(struct hrtimer *timer_for_restart)
{
    struct CtrlReqMsg ctrl_req;
    struct CtrlRepMsg ctrl_rep;
    struct timespec ts_now;
    static unsigned int step_pos = 0;
    /* Timestamp system clock */
    getnstimeofday(&ts_now);

    if (pru_comm_get_ctrl_request(&ctrl_req))
    {
        if (ctrl_req.identifier != MSG_SYNC_CTRL_REQ)
        {
            /* Error occurs if something writes over boundaries */
            printk(KERN_INFO "shprd: Kernel Recv_CtrlRequest -> mem corruption?\n");
        }

        sync_loop(&ctrl_rep, &ctrl_req);
        ctrl_rep.identifier = MSG_SYNC_CTRL_REP;
        ctrl_rep.msg_unread = 1;

        if (!pru_com_set_ctrl_reply(&ctrl_rep))
        {
            /* Error occurs if PRU was not able to handle previous message in time */
            printk(KERN_INFO "shprd: Kernel Send_CtrlResponse -> back-pressure\n");
        }

        /* resetting to longest sleep period */
        step_pos = 0;
    }

    hrtimer_forward(timer_for_restart, timespec_to_ktime(ts_now),
            ns_to_ktime(timer_steps_ns[step_pos])); /* variable sleep cycle */
    /* TODO: ktime_get() seems a proper replacement for "timespec_to_ktime(ts_now)" */

    if (step_pos < timer_steps_ns_size - 1) step_pos++;

    return HRTIMER_RESTART;
}


int sync_loop(struct CtrlRepMsg *const ctrl_rep, const struct CtrlReqMsg *const ctrl_req)
{
	int64_t ns_iep_to_wrap;
	int32_t clock_corr;
	uint64_t ns_per_tick;

	/*
     * Based on the previous IEP timer period and the nominal timer period
     * we can estimate the real nanoseconds passing per tick
     * We operate on fixed point arithmetics by shifting by 32 bit
     */
	ns_per_tick = div_u64(((uint64_t)trigger_timer.timer_period_ns << 32),
			      ctrl_req->old_period);

	/*
     * Get distance of IEP clock at interrupt from timer wrap
     * negative, if interrupt happened before wrap, positive after
     */
	ns_iep_to_wrap = ((int64_t)ctrl_req->ticks_iep) * ns_per_tick;
	if (ns_iep_to_wrap > ((uint64_t)trigger_timer.timer_period_ns << 31)) {
		ns_iep_to_wrap =
			ns_iep_to_wrap -
			((uint64_t)trigger_timer.timer_period_ns << 32);
	}

	/* Difference between system clock and IEP clock phase */
	sync_data->err = ((int64_t)ns_iep_to_wrap - ns_sys_to_wrap) >> 32;
	sync_data->err_sum += sync_data->err;

	/* This is the actual PI controller equation */
	clock_corr =
		div_s64(sync_data->err, 32) + div_s64(sync_data->err_sum, 128);

	ctrl_rep->clock_corr = clock_corr;
	ctrl_rep->next_timestamp_ns = next_timestamp_ns;

	sync_data->clock_corr = ctrl_rep->clock_corr;

	return 0;
}