#pragma once

#include <stdint.h>

#define TRUE  1
#define FALSE 0

void virtcap_init(uint32_t upper_threshold_voltage_param, uint32_t lower_threshold_voltage_param, uint32_t capacitance_uF);
void virtcap_update(uint32_t current_measured, uint32_t voltage_measured);

