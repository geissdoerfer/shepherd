#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/io.h>

#include "pru_comm.h"
#include "commons.h"

#define PRU_BASE_ADDR 0x4A300000
#define PRU_INTC_OFFSET 0x00020000
#define PRU_INTC_SIZE 0x400
#define PRU_INTC_SISR_OFFSET 0x20

static void __iomem *pru_intc_io = NULL;
void __iomem *pru_shared_mem_io = NULL;

/* This timer is used to schedule a delayed start of the actual sampling on the PRU */
struct hrtimer delayed_start_timer;

static enum hrtimer_restart
delayed_start_callback(struct hrtimer *timer_for_restart);

int pru_comm_init(void)
{
	/* Maps the control registers of the PRU's interrupt controller */
	pru_intc_io = ioremap(PRU_BASE_ADDR + PRU_INTC_OFFSET, PRU_INTC_SIZE);
	/* Maps the shared memory in the shared DDR, used to exchange info/control between PRU cores and kernel */
	pru_shared_mem_io =
		ioremap(PRU_BASE_ADDR + PRU_SHARED_MEM_STRUCT_OFFSET,
			sizeof(struct SharedMem));

	hrtimer_init(&delayed_start_timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	delayed_start_timer.function = &delayed_start_callback;

	return 0;
}

int pru_comm_exit(void)
{
	iounmap(pru_intc_io);
	iounmap(pru_shared_mem_io);

	return 0;
}

static enum hrtimer_restart
delayed_start_callback(struct hrtimer *timer_for_restart)
{
	struct timespec ts_now;
	uint64_t now_ns_system;

	pru_comm_trigger(HOST_PRU_EVT_START);

	/* Timestamp system clock */
	getnstimeofday(&ts_now);

	now_ns_system = (uint64_t)timespec_to_ns(&ts_now);

	printk(KERN_INFO "shprd: Triggered delayed start@%llu", now_ns_system);
	return HRTIMER_NORESTART;
}

int pru_comm_schedule_delayed_start(unsigned int start_time_second)
{
	ktime_t trigger_timer_time;
	uint64_t trigger_timer_time_ns;

	trigger_timer_time = ktime_set((const s64)start_time_second, 0);

	/**
     * The timer should fire in the middle of the interval before we want to 
     * start. This allows the PRU enough time to receive the interrupt and
     * prepare itself to start at exactly the right time.
     */
	trigger_timer_time = ktime_sub_ns(
		trigger_timer_time, 3 * pru_comm_get_buffer_period_ns() / 4);

	trigger_timer_time_ns = ktime_to_ns(trigger_timer_time);

	printk(KERN_INFO "shprd: Delayed start timer set to %llu",
	       trigger_timer_time_ns);

	hrtimer_start(&delayed_start_timer, trigger_timer_time,
		      HRTIMER_MODE_ABS);

	return 0;
}

int pru_comm_cancel_delayed_start(void)
{
	return hrtimer_cancel(&delayed_start_timer);
}

int pru_comm_trigger(unsigned int system_event)
{
	/* Raise Interrupt on PRU INTC*/
	writel(system_event, pru_intc_io + PRU_INTC_SISR_OFFSET);

	return 0;
}

enum ShepherdState pru_comm_get_state(void)
{
	return (enum ShepherdState)readl(
		pru_shared_mem_io + offsetof(struct SharedMem, shepherd_state));
}

int pru_comm_set_state(enum ShepherdState state)
{
	writel(state,
	       pru_shared_mem_io + offsetof(struct SharedMem, shepherd_state));
	return 0;
}

unsigned int pru_comm_get_buffer_period_ns(void)
{
	return readl(pru_shared_mem_io +
		     offsetof(struct SharedMem, buffer_period_ns));
}