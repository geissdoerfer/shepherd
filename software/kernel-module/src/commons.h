#ifndef __COMMONS_H_
#define __COMMONS_H_
// NOTE: a similar version of this definition-file exists for the pru-firmware (copy changes by hand)

/**
 * These are the system events that we use to signal events to the PRUs.
 * See the AM335x TRM Table 4-22 for a list of all events
 */
#define HOST_PRU_EVT_TIMESTAMP 20

/* The SharedMem struct resides at the beginning of the PRUs shared memory */
#define PRU_SHARED_MEM_STRUCT_OFFSET 0x10000

enum SyncMsg { MSG_SYNC_CTRL_REQ = 0xF0, MSG_SYNC_CTRL_REP = 0xF1 };

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
	/* ADC calibration settings */
	struct CalibrationSettings calibration_settings;
	/* This structure defines all settings of virtcap emulation*/
	struct VirtCapSettings virtcap_settings;
} __attribute__((packed));

/* Control request message sent from PRU1 to this kernel module */
struct CtrlReqMsg {
	/* This is used to identify message type at receiver */
	char identifier;
	/* Number of ticks passed on the PRU's IEP timer */
	uint32_t ticks_iep;
	/* Previous buffer period in IEP ticks */
	uint32_t old_period;
} __attribute__((packed));

/* Control reply message sent from this kernel module to PRU1 after running the control loop */
struct CtrlRepMsg {
	char identifier;
	int32_t clock_corr;
	uint64_t next_timestamp_ns;
} __attribute__((packed));

#endif /* __COMMONS_H_ */