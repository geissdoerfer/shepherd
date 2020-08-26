#ifndef __SAMPLING_H_
#define __SAMPLING_H_

#include "commons.h"

void sampling_init(enum ShepherdMode mode, unsigned int harvesting_voltage);
void sample(struct SampleBuffer *current_buffer, unsigned int sample_idx,
	    enum ShepherdMode mode);
unsigned int sample_dbg_adc(unsigned int channel_no);
void sample_dbg_dac(unsigned int value);

#endif /* __SAMPLING_H_ */
