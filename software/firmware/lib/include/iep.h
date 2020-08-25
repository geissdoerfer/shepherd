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
void iep_set_us(uint32_t value);
void iep_reset();
void iep_start();
void iep_stop();
inline int8_t iep_check_evt_cmp(uint32_t compare_channel);
inline void iep_clear_evt_cmp(uint32_t compare_channel);
void iep_enable_evt_cmp(uint32_t compare_channel);
void iep_disable_evt_cmp(uint32_t compare_channel);
void iep_set_cmp_val(uint32_t compare_channel, uint32_t value);
uint32_t iep_get_cmp_val(uint32_t compare_channel);


#endif /* __IEP_H_ */
