#ifndef __SYNC_CTRL_H_
#define __SYNC_CTRL_H_

#define MSG_ID_CTRL_REQ 0xF0
#define MSG_ID_CTRL_REP 0xF1

/* Control request message sent from PRU0 to this kernel module */
struct msg_ctrl_req_s {
    /* This is used to identify message type at receiver */
    char identifier;
    /* Number of ticks passed on the PRU's IEP timer */
    uint32_t ticks_iep;
    /* Previous buffer period in IEP ticks */
    uint32_t old_period;
} __attribute__((packed));

/* Control reply message sent from this kernel module to PRU0 after running the control loop */
struct msg_ctrl_rep_s {
    char identifier;
    int32_t clock_corr;
    uint64_t next_timestamp_ns;
} __attribute__((packed));

/**
 * Initializes snychronization procedure between our Linux clock and PRU0
 * 
 * This initializes and starts the timer that fires with a period corresponding
 * to the 'buffer period' and a phase aligned with the real time. This timer
 * triggers an interrupt on PRU0
 */
int sync_init(uint32_t timer_period_ns);
int sync_exit(void);
int sync_reset(void);

/**
 * Control loop 
 * 
 * The controller is best described as a Phase-Locked-Loop system: The kernel
 * module runs a reference clock with phase and frequency synchronized to the
 * network-wide master. The frequency equals the 'buffer period' and the phase
 * should be aligned to the wrap of the real time. E.g. if we have a buffer
 * period of 100ms, the timer should fire at X.1s, X.2s, X.3s and so on.
 * Our task is to copy that clock to the PRU's IEP. For this purpose, we run
 * a Linux hrtimer, that expires on the corresponding wrap of the Linux
 * CLOCK_REALTIME and we immediately trigger an interrupt on the PRU. The PRU
 * sends us its own phase via RPMSG. The goal of this function is to calculate
 * a 'correction factor' that is added to the IEP's frequency, such that the
 * difference between the phase of our clock and the IEP's is minimized.
 * 
 * @param ctrl_rep Buffer to store the result of the control loop
 * @param ctrl_req Control request that was received from PRU0
 */
int sync_loop(struct msg_ctrl_rep_s * ctrl_rep, struct msg_ctrl_req_s * ctrl_req);

#endif /* __SYNC_CTRL_H_ */
