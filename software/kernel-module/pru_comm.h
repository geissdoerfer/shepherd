#ifndef PRU_COMM_H_
#define PRU_COMM_H_

/**
 * These are the system events that we use to signal events to the PRUs.
 * See the AM335x TRM Table 4-22 for a list of all events
 */
#define HOST_PRU_EVT_TIMESTAMP 29
#define HOST_PRU_EVT_START 28
#define HOST_PRU_EVT_RESET 27

/* The SharedMem struct resides at the beginning of the PRUs shared memory */
#define PRU_SHARED_MEM_STRUCT_OFFSET 0x10000

/* This is external to expose some of the attributes through sysfs */
extern void __iomem * pru_shared_mem_io;

enum ShepherdMode{MODE_HARVESTING, MODE_LOAD, MODE_EMULATION, MODE_DEBUG};
enum ShepherdState{STATE_UNKNOWN, STATE_IDLE, STATE_ARMED, STATE_RUNNING, STATE_FAULT};

struct SharedMem
{
	uint32_t shepherd_state;
	/* Stores the mode, e.g. harvesting or emulation */
	uint32_t shepherd_mode;
	/* Allows setting a fixed harvesting voltage as reference for the boost converter */
	uint32_t harvesting_voltage;
	/* Physical address of shared area in DDR RAM, that is used to exchange data between user space and PRUs */
	uint32_t mem_base_addr;
	/* Length of shared area in DDR RAM */
	uint32_t mem_size;
	/* Maximum number of buffers stored in the shared DDR RAM area */
	uint32_t n_buffers;
	/* Number of IV samples stored per buffer */
	uint32_t samples_per_buffer;
	/* The time for sampling samples_per_buffer. Determines sampling rate */
	uint32_t buffer_period_ns;
}__attribute__((packed));

/**
 * Initializes communication between our kernel module and the PRUs.
 * 
 * Maps the PRU's interrupt controller's memory to allow triggering system
 * events, causing 'interrupts' on the PRUs.
 * Maps the shared memory structure within the PRU's 'shared RAM' memory
 * region.
 */
int pru_comm_init(void);
/**
 * Clean up communication between our kernel module and the PRUs.
 * 
 * @see pru_comm_init()
 */
int pru_comm_exit(void);

/**
 * Trigger a system event on the PRUs
 * 
 * The PRUs have an interrupt controller (INTC), which connects 64 so-called
 * 'system events' to the PRU's interrupt system. We use some of these system
 * events to communicate time-critical events from this Linux kernel module to
 * the PRUs
 */
int pru_comm_trigger(unsigned int system_event);

/**
 * Schedule start of the actual sampling at a later point in time
 * 
 * It is hard to execute a command simultaneously on a set of Linux hosts.
 * This is however necessary, especially for emulation, where all shepherd
 * nodes should start replaying samples at the same time. This function allows
 * to register a trigger at a defined time with respect to the CLOCK_REALTIME.
 * 
 * @param start_time_second desired system time in seconds at which PRUs should start sampling/replaying
 */ 
int pru_comm_schedule_delayed_start(unsigned int start_time_second);

/**
 * Cancel a previously scheduled 'delayed start'
 * 
 * @see pru_comm_trigger()
 */
int pru_comm_cancel_delayed_start(void);

/**
 * Read the 'shepherd state' from the PRUs
 * 
 * This kernel module usually requests state changes from the PRU. By reading
 * the state from the shared memory structure, we can check in which state
 * the PRUs actually are.
 */
enum ShepherdState pru_comm_get_state(void);
/**
 * Set the 'shepherd state'
 * 
 * When scheduling a delayed start, it is necessary that we directly change the
 * shepherd state from within the kernel module by directly writing the
 * corresponding value to the shared memory structure
 * 
 * @param state new shepherd state
 * @see SharedMem
 */
int pru_comm_set_state(enum ShepherdState state);

/**
 * Reads the buffer period from the PRUs
 * 
 * The 'buffer period' is a crucial parameter that, together with the number
 * of samples per buffer determines the sampling rate. It is defined in the
 * PRU firmware, but we need it to set the timer period that is used to
 * schedule samples on the PRUs.
 * 
 * @see sync_init()
 * 
 * @returns Buffer period in nanoseconds
 */
unsigned int pru_comm_get_buffer_period_ns(void);

#endif /* PRU_COMM_H_ */
