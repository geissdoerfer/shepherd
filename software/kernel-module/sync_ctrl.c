#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/math64.h>

#include "sync_ctrl.h"
#include "pru_comm.h"

static int64_t ns_sys_to_wrap;
static uint64_t next_timestamp_ns;
static int64_t err_sum;

static enum hrtimer_restart timer_callback( struct hrtimer * timer_for_restart);

/* We wrap the timer to pass arguments to the callback function */
static struct hrtimer_wrap_s {
    uint32_t timer_period_ns;
    struct hrtimer hr_timer;
} trigger_timer;

int sync_exit(void)
{
    hrtimer_cancel(&trigger_timer.hr_timer);
    return 0;
}

int sync_init(uint32_t timer_period_ns)
{
    struct timespec ts_now;
    uint64_t now_ns_system;
    uint32_t ns_over_wrap;
    uint64_t ns_now_until_trigger;

    trigger_timer.timer_period_ns = timer_period_ns;

    hrtimer_init(&trigger_timer.hr_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
    trigger_timer.hr_timer.function = &timer_callback;

    /* Timestamp system clock */
    getnstimeofday (&ts_now);

    now_ns_system = (uint64_t) timespec_to_ns(&ts_now);

    div_u64_rem(now_ns_system, timer_period_ns, &ns_over_wrap);
    if(ns_over_wrap > (timer_period_ns / 2))
        ns_now_until_trigger = 2 * timer_period_ns - ns_over_wrap;
    else
        ns_now_until_trigger = timer_period_ns - ns_over_wrap;

    hrtimer_start(
        &trigger_timer.hr_timer, ns_to_ktime(now_ns_system + ns_now_until_trigger), HRTIMER_MODE_ABS);

    return 0;
}

int sync_reset(void)
{
    err_sum = 0;
    return 0;
}

enum hrtimer_restart timer_callback( struct hrtimer * timer_for_restart)
{
    struct timespec ts_now;
    uint64_t now_ns_system;
    uint32_t ns_over_wrap;
    uint64_t ns_now_until_trigger;

    struct hrtimer_wrap_s * wrapped_timer;

    wrapped_timer = container_of(timer_for_restart, struct hrtimer_wrap_s, hr_timer);

    /* Raise Interrupt on PRU, telling it to timestamp IEP */
    pru_comm_trigger(HOST_PRU_EVT_TIMESTAMP);
    /* Timestamp system clock */
    getnstimeofday (&ts_now);

    now_ns_system = (uint64_t) timespec_to_ns(&ts_now);

    /*
     * Get distance of system clock from timer wrap.
     * Is negative, when interrupt happened before wrap, positive when after
     */
    div_u64_rem(now_ns_system, wrapped_timer->timer_period_ns, &ns_over_wrap);
    if(ns_over_wrap > (wrapped_timer->timer_period_ns / 2)) {
        ns_sys_to_wrap = ((int64_t) ns_over_wrap - wrapped_timer->timer_period_ns) << 32;
        next_timestamp_ns = now_ns_system - ns_over_wrap + 2 * wrapped_timer->timer_period_ns;
        ns_now_until_trigger = 2 * wrapped_timer->timer_period_ns - ns_over_wrap;
    }
    else {
        ns_sys_to_wrap = ((int64_t) ns_over_wrap) << 32;
        next_timestamp_ns = now_ns_system - ns_over_wrap + wrapped_timer->timer_period_ns;
        ns_now_until_trigger = wrapped_timer->timer_period_ns - ns_over_wrap;
    }

    hrtimer_forward(
        timer_for_restart,
        timespec_to_ktime(ts_now),
        ns_to_ktime(ns_now_until_trigger)
    );

    return HRTIMER_RESTART;
}

int sync_loop(struct msg_ctrl_rep_s * ctrl_rep, struct msg_ctrl_req_s * ctrl_req)
{
    int64_t ns_iep_to_wrap;
    int64_t err;
    int32_t clock_corr;
    uint64_t ns_per_tick;
    int32_t clock_corr_max;

    /*
     * Based on the previous IEP timer period and the nominal timer period
     * we can estimate the real nanoseconds passing per tick
     * We operate on fixed point arithmetics by shifting by 32 bit
     */
    ns_per_tick = div_u64(((uint64_t) trigger_timer.timer_period_ns << 32), ctrl_req->old_period);

    /*
     * Get distance of IEP clock at interrupt from timer wrap
     * negative, if interrupt happened before wrap, positive after
     */
    ns_iep_to_wrap = ((int64_t) ctrl_req->ticks_iep) * ns_per_tick;
    if(ns_iep_to_wrap > ((uint64_t) trigger_timer.timer_period_ns << 31)) {
        ns_iep_to_wrap = ns_iep_to_wrap - ((uint64_t) trigger_timer.timer_period_ns << 32);
    }

    /* Difference between system clock and IEP clock phase */
    err = ((int64_t) ns_iep_to_wrap - ns_sys_to_wrap) >> 32;
    err_sum += err;

    /* This is the actual PI controller equation */
    clock_corr = div_s64(err, 32) + div_s64(err_sum, 512);

    /* Clamp maximum correction to 10% of previous timer period */
    clock_corr_max = (int32_t) ctrl_req->old_period / 100;
    if(clock_corr > clock_corr_max)
        ctrl_rep->clock_corr = clock_corr_max;
    else if(clock_corr < -clock_corr_max)
        ctrl_rep->clock_corr = - clock_corr_max;
    else
        ctrl_rep->clock_corr = clock_corr;

    ctrl_rep->next_timestamp_ns = next_timestamp_ns;
    ctrl_rep->identifier = MSG_ID_CTRL_REP;

    printk(KERN_INFO "shprd: ERR: %lld CORR: %d", err, ctrl_rep->clock_corr);

    return 0;

}