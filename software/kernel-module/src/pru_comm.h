#ifndef PRU_COMM_H_
#define PRU_COMM_H_

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

/**
 * Receives Ctrl-Messages from PRU1
 * @param ctrl_request
 * @return success = 1, error = 0
 */
unsigned char pru_comm_get_ctrl_request(struct CtrlReqMsg *const ctrl_request);
/**
 * Sends Ctrl-Messages to PRU1
 * @param ctrl_reply
 * @return success = 1, error = 0
 */
unsigned char pru_com_set_ctrl_reply(const struct CtrlRepMsg *const ctrl_reply);


#endif /* PRU_COMM_H_ */
