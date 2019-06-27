#ifndef __PRU_COMM_FORMAT_H_
#define __PRU_COMM_FORMAT_H_

#include "simple_lock.h"

#define PRU_PRU_EVT_SAMPLE 30
#define PRU_PRU_EVT_BLOCK_END 31

#define HOST_PRU_EVT_TIMESTAMP 29
#define HOST_PRU_EVT_START 28
#define HOST_PRU_EVT_RESET 27

#define PRU_SHARED_MEM_STRUCT_OFFSET 0x10000

#define MAX_GPIO_EVT_PER_BUFFER 16384

enum ShepherdMode { MODE_HARVESTING, MODE_LOAD, MODE_EMULATION, MODE_DEBUG };
enum ShepherdState {
	STATE_UNKNOWN,
	STATE_IDLE,
	STATE_ARMED,
	STATE_RUNNING,
	STATE_FAULT
};

struct gpio_edges_s {
	uint32_t idx;
	uint64_t timestamp_ns[MAX_GPIO_EVT_PER_BUFFER];
	char bitmask[MAX_GPIO_EVT_PER_BUFFER];
} __attribute__((packed));

struct SharedMem {
	uint32_t shepherd_state;
	uint32_t shepherd_mode;
	uint32_t harvesting_voltage;
	uint32_t mem_base_addr;
	uint32_t mem_size;
	uint32_t n_buffers;
	uint32_t samples_per_buffer;
	uint32_t buffer_period_ns;
	uint64_t next_timestamp_ns;
	simple_mutex_t gpio_edges_mutex;
	struct gpio_edges_s *gpio_edges;
} __attribute__((packed));

#endif /* __PRU_COMM_FORMAT_H_ */
