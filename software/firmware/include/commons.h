#ifndef __COMMONS_H_
#define __COMMONS_H_

#include "simple_lock.h"
#include "shepherd_config.h"

#define HOST_PRU_EVT_TIMESTAMP 20

#define PRU_PRU_EVT_SAMPLE 30
#define PRU_PRU_EVT_BLOCK_END 31

#define PRU_SHARED_MEM_STRUCT_OFFSET 0x10000

#define MAX_GPIO_EVT_PER_BUFFER 16384

/* Message IDs used in Data Exchange Protocol between PRU0 and user space */
enum DEPMsgID {
	MSG_DEP_ERROR = 0,
	MSG_DEP_BUF_FROM_HOST = 1,
	MSG_DEP_BUF_FROM_PRU = 2,
	MSG_DEP_ERR_INCMPLT = 3,
	MSG_DEP_ERR_INVLDCMD = 4,
	MSG_DEP_ERR_NOFREEBUF = 5,
	MSG_DEP_DBG_PRINT = 6,
	MSG_DEP_DBG_ADC = 0xF0,
	MSG_DEP_DBG_DAC = 0xF1
};

/* Message IDs used in Synchronization Protocol between PRU1 and kernel module */
enum SyncMsgID { MSG_SYNC_CTRL_REQ = 0xF0, MSG_SYNC_CTRL_REP = 0xF1 };

enum ShepherdMode {
	MODE_HARVESTING,
	MODE_LOAD,
	MODE_EMULATION,
	MODE_VIRTCAP,
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
	unsigned int idx;
	uint64_t timestamp_ns[MAX_GPIO_EVT_PER_BUFFER];
    unsigned char bitmask[MAX_GPIO_EVT_PER_BUFFER];
} __attribute__((packed));

struct SampleBuffer {
	unsigned int len;
	uint64_t timestamp_ns;
	unsigned int values_voltage[SAMPLES_PER_BUFFER];
	unsigned int values_current[SAMPLES_PER_BUFFER];
	struct GPIOEdges gpio_edges;
} __attribute__((packed));

struct CalibrationSettings {
	/* Gain of load current adc. It converts current to adc value */
	int adc_load_current_gain;
	/* Offset of load current adc */
	int adc_load_current_offset;
	/* Gain of load voltage adc. It converts voltage to adc value */
	int adc_load_voltage_gain;
	/* Offset of load voltage adc */
	int adc_load_voltage_offset;
} __attribute__((packed));

/* This structure defines all settings of virtcap emulation*/
struct VirtCapSettings {
  int upper_threshold_voltage;
  int lower_threshold_voltage;
  int sample_period_us;
  int capacitance_uf;
  int max_cap_voltage;
  int min_cap_voltage;
  int init_cap_voltage;
  int dc_output_voltage;
  int leakage_current;
  int discretize;
  int output_cap_uf;
  int lookup_input_efficiency[4][9];
  int lookup_output_efficiency[4][9];
} __attribute__((packed));

/* Format of RPMSG used in Data Exchange Protocol between PRU0 and user space */
struct DEPMsg {
	unsigned int msg_type;
	unsigned int value;
} __attribute__((packed));

/* Format of memory structure shared between PRU0, PRU1 and kernel module */
struct SharedMem {
	unsigned int shepherd_state;
	/* Stores the mode, e.g. harvesting or emulation */
	unsigned int shepherd_mode;
	/* Allows setting a fixed harvesting voltage as reference for the boost converter */
	unsigned int harvesting_voltage;
	/* Physical address of shared area in DDR RAM, that is used to exchange data between user space and PRUs */
	unsigned int mem_base_addr;
	/* Length of shared area in DDR RAM */
	unsigned int mem_size;
	/* Maximum number of buffers stored in the shared DDR RAM area */
	unsigned int n_buffers;
	/* Number of IV samples stored per buffer */
	unsigned int samples_per_buffer;
	/* The time for sampling samples_per_buffer. Determines sampling rate */
	unsigned int buffer_period_ns;
	/* ADC calibration settings */
	struct CalibrationSettings calibration_settings;
	/* This structure defines all settings of virtcap emulation*/
	struct VirtCapSettings virtcap_settings;
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
} __attribute__((packed));

/* Format of RPMSG message sent from PRU1 to kernel module */
struct CtrlReqMsg {
	/* This is used to identify message type at receiver */
    unsigned char identifier;
	/* Number of ticks passed on the PRU's IEP timer */
	unsigned int ticks_iep;
	/* Previous buffer period in IEP ticks */
	unsigned int old_period;
} __attribute__((packed));

/* Format of RPMSG message sent from kernel module to PRU1 */
struct CtrlRepMsg {
    unsigned char identifier;
	int clock_corr;
	uint64_t next_timestamp_ns;
} __attribute__((packed));

struct ADCReading {
	int current;
	int voltage;
};


#endif /* __COMMONS_H_ */
