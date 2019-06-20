#ifndef __RPMSG_FORMAT_H_
#define __RPMSG_FORMAT_H_

#include "pru_comm_format.h"

#define VALUES_PER_BUFFER 20000

#define MSG_ERROR 0
#define MSG_BUFFER_FROM_HOST 1
#define MSG_BUFFER_FROM_PRU 2

#define MSG_ERR_INCOMPLETE 3
#define MSG_ERR_INVALIDCMD 4
#define MSG_ERR_NOFREEBUF 5

#define MSG_DBG_ADC 0xF0
#define MSG_DBG_DAC 0xF1

struct rpmsg_msg_s
{
	  uint32_t msg_type;
	  uint32_t value;
}__attribute__((packed));

struct SampleBuffer
{
		uint32_t len;
		uint64_t timestamp;
		uint32_t values_voltage[VALUES_PER_BUFFER / 2];
		uint32_t values_current[VALUES_PER_BUFFER / 2];
		struct gpio_edges_s gpio_edges;
}__attribute__((packed));

#endif /* __RPMSG_FORMAT_H_ */
