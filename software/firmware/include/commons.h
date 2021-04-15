#ifndef __COMMONS_H_
#define __COMMONS_H_
// NOTE: a (almost)Copy of this definition-file exists for the kernel module (copy changes by hand)
// NOTE: and most of the structs are hardcoded in read_buffer() in shepherd_io.py

#include "simple_lock.h"
#include "shepherd_config.h"
#include "stdint_fast.h"

/**
 * These are the system events that we use to signal events to the PRUs.
 * See the AM335x TRM Table 4-22 for a list of all events
 */
#define HOST_PRU_EVT_TIMESTAMP          (20u)

/* The SharedMem struct resides at the beginning of the PRUs shared memory */
#define PRU_SHARED_MEM_STRUCT_OFFSET    (0x10000u)

/* gpio_buffer_size that comes with every analog_sample_buffer (0.1s) */
#define MAX_GPIO_EVT_PER_BUFFER         (16384u)

// Test data-containers and constants with pseudo-assertion with zero cost (if expression evaluates to 0 this causes a div0
// NOTE: name => alphanum without spaces and without ""
#define ASSERT(name, expression) 	extern uint32_t assert_name[1/(expression)]

/* Message IDs used in Data Exchange Protocol between PRU0 and user space */
enum DEPMsgID {
	MSG_DEP_ERROR = 0u,
	MSG_DEP_BUF_FROM_HOST = 1u,
	MSG_DEP_BUF_FROM_PRU = 2u,
	MSG_DEP_ERR_INCMPLT = 3u,
	MSG_DEP_ERR_INVLDCMD = 4u,
	MSG_DEP_ERR_NOFREEBUF = 5u,
	MSG_DEP_DBG_PRINT = 6u,
	MSG_DEP_DBG_ADC = 0xF0u,
	MSG_DEP_DBG_DAC = 0xF1u
};

/* Message IDs used in Synchronization Protocol between PRU1 and kernel module */
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

struct GPIOEdges {
	uint32_t idx;
	uint64_t timestamp_ns[MAX_GPIO_EVT_PER_BUFFER];
    uint8_t  bitmask[MAX_GPIO_EVT_PER_BUFFER];
} __attribute__((packed));

struct SampleBuffer {
	uint32_t len;
	uint64_t timestamp_ns;
	uint32_t values_voltage[ADC_SAMPLES_PER_BUFFER]; // TODO: would be more efficient for PRU to keep matching V&C together
	uint32_t values_current[ADC_SAMPLES_PER_BUFFER];
	struct GPIOEdges gpio_edges;
} __attribute__((packed));

/* Format of RPMSG used in Data Exchange Protocol between PRU0 and user space */
struct DEPMsg {
	uint32_t msg_type;
	uint32_t value;
} __attribute__((packed));

/* Format of RPMSG message sent from PRU1 to kernel module */
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

/* Format of RPMSG message sent from kernel module to PRU1 */
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

/* Format of memory structure shared between PRU0, PRU1 and kernel module (lives in shared RAM of PRUs) */
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
	/* NOTE: End of region (also) controlled by kernel module */

	/* Used to exchange timestamp of next buffer between PRU1 and PRU0 */
	uint64_t next_timestamp_ns;
	/* Protects write access to below gpio_edges structure */
	simple_mutex_t gpio_edges_mutex;
	/**
	* Pointer to gpio_edges structure in current buffer. Only PRU0 knows about
	* which is the current buffer, but PRU1 is sampling GPIOs. Therefore PRU0
	* shares the memory location of the current gpio_edges struct
	*/
	struct GPIOEdges *gpio_edges;
	/* Counter for ADC-Samples, updated by PRU0, also needed (non-writing) by PRU1 for some timing-calculations */
	uint32_t analog_sample_counter;
	/* Token system to ensure both PRUs can share interrupts */
	bool_ft cmp0_trigger_for_pru1;
	bool_ft cmp1_trigger_for_pru1;
} __attribute__((packed));

ASSERT(shared_mem_size, sizeof(struct SharedMem) < 10000);

struct ADCReading {
	int32_t current;
	int32_t voltage;
};


#endif /* __COMMONS_H_ */
