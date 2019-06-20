#ifndef __SAMPLING_H_
#define __SAMPLING_H_

#include "pru_comm_format.h"
#include "rpmsg_format.h"

void sampling_init(enum shepherd_mode_e mode, unsigned int harvesting_voltage);
void sample(struct SampleBuffer * current_buffer, unsigned int sample_idx, enum shepherd_mode_e mode);

#endif /* __SAMPLING_H_ */
