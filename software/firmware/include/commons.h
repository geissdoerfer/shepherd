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
	uint32_t idx;
	uint64_t timestamp_ns[MAX_GPIO_EVT_PER_BUFFER];
	char bitmask[MAX_GPIO_EVT_PER_BUFFER];
} __attribute__((packed));

struct SampleBuffer {
	uint32_t len;
	uint64_t timestamp_ns;
	uint32_t values_voltage[SAMPLES_PER_BUFFER];
	uint32_t values_current[SAMPLES_PER_BUFFER];
	struct GPIOEdges gpio_edges;
} __attribute__((packed));

struct CalibrationSettings {
	/* Gain of load current adc. It converts current to adc value */
	int32_t adc_load_current_gain;
	/* Offset of load current adc */
	int32_t adc_load_current_offset;
	/* Gain of load voltage adc. It converts voltage to adc value */
	int32_t adc_load_voltage_gain;
	/* Offset of load voltage adc */
	int32_t adc_load_voltage_offset;
} __attribute__((packed));

/* This structure defines all settings of virtcap emulation*/
struct VirtCapSettings {
  int32_t upper_threshold_voltage;
  int32_t lower_threshold_voltage;
  int32_t sample_period_us;
  int32_t capacitance_uf;
  int32_t max_cap_voltage;
  int32_t min_cap_voltage;
  int32_t init_cap_voltage;
  int32_t dc_output_voltage;
  int32_t leakage_current;
  int32_t discretize;
  int32_t output_cap_uf;
  int32_t lookup_input_efficiency[4][9];
  int32_t lookup_output_efficiency[4][9];
} __attribute__((packed));

/* Format of RPMSG used in Data Exchange Protocol between PRU0 and user space */
struct DEPMsg {
	uint32_t msg_type;
	uint32_t value;
} __attribute__((packed));

/* Format of memory structure shared between PRU0, PRU1 and kernel module */
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
	char identifier;
	/* Number of ticks passed on the PRU's IEP timer */
	uint32_t ticks_iep;
	/* Previous buffer period in IEP ticks */
	uint32_t old_period;
} __attribute__((packed));

/* Format of RPMSG message sent from kernel module to PRU1 */
struct CtrlRepMsg {
	char identifier;
	int32_t clock_corr;
	uint64_t next_timestamp_ns;
} __attribute__((packed));

#endif /* __COMMONS_H_ */
