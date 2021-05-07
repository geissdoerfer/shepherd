#ifndef __COMMONS_H_
#define __COMMONS_H_
// NOTE: a (almost)Copy of this definition-file exists for the pru-firmware (copy changes by hand)

/**
 * These are the system events that we use to signal events to the PRUs.
 * See the AM335x TRM Table 4-22 for a list of all events
 */
#define HOST_PRU_EVT_TIMESTAMP      (20)

/* The SharedMem struct resides at the beginning of the PRUs shared memory */
#define PRU_SHARED_MEM_STRUCT_OFFSET 0x10000

enum SyncMsgID { MSG_SYNC_CTRL_REQ = 0x55, MSG_SYNC_CTRL_REP = 0xAA };

enum ShepherdMode {
	MODE_HARVESTING,
	MODE_LOAD,
	MODE_EMULATION,
	MODE_DEBUG
};
enum ShepherdState {
	STATE_UNKNOWN,
	STATE_IDLE,
	STATE_ARMED,
	STATE_RUNNING,
	STATE_RESET,
	STATE_FAULT
};


/* Control request message sent from PRU1 to this kernel module */
struct CtrlReqMsg {
    /* Identifier => Canary, This is used to identify memory corruption */
	uint8_t identifier;
	/* Token-System to signal new message & the ack, (sender sets unread, receiver resets) */
	uint8_t msg_unread;
    /* Alignment with memory, (bytes)mod4 */
	uint8_t reserved[2];
	/* Number of ticks passed on the PRU's IEP timer */
	uint32_t ticks_iep;
} __attribute__((packed));


/* Control reply message sent from this kernel module to PRU1 after running the control loop */
struct CtrlRepMsg {
	/* Identifier => Canary, This is used to identify memory corruption */
	uint8_t identifier;
    /* Token-System to signal new message & the ack, (sender sets unread, receiver resets) */
    uint8_t msg_unread;
    /* Alignment with memory, (bytes)mod4 */
	uint8_t reserved0[2];
	/* Actual Content of message */
    uint32_t buffer_block_period;   // corrected ticks that equal 100ms
    uint32_t analog_sample_period;  // ~ 10 us
    uint32_t compensation_steps;    // remainder of buffer_block/sample_count = sample_period
    uint64_t next_timestamp_ns;     // start of next buffer block
} __attribute__((packed));


/* This is external to expose some of the attributes through sysfs */
extern void __iomem *pru_shared_mem_io;

struct SharedMem {
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
	/* replacement Msg-System for slow rpmsg (check 640ns, receive 4820ns) */
	struct CtrlReqMsg ctrl_req;
	struct CtrlRepMsg ctrl_rep;
} __attribute__((packed));

#endif /* __COMMONS_H_ */