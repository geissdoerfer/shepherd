#ifndef __IEP_H_
#define __IEP_H_

#include <stdint.h>

#define IEP_CMP0 0
#define IEP_CMP1 1
#define IEP_CMP2 2
#define IEP_CMP3 3
#define IEP_CMP4 4
#define IEP_CMP5 5
#define IEP_CMP6 6
#define IEP_CMP7 7


void iep_init();
void iep_set_us(unsigned int value);
void iep_reset();
void iep_start();
void iep_stop();
inline int iep_check_evt_cmp(unsigned int compare_channel);
inline void iep_clear_evt_cmp(unsigned int compare_channel);
void iep_enable_evt_cmp(unsigned int compare_channel);
void iep_disable_evt_cmp(unsigned int compare_channel);
void iep_set_cmp_val(unsigned int compare_channel, unsigned int value);
unsigned int iep_get_cmp_val(unsigned int compare_channel);


#endif /* __IEP_H_ */
