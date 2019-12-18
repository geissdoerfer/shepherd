#pragma once

#include <stdint.h>
#include "commons.h"

#define TRUE 1
#define FALSE 0

#define SHIFT_VOLT 12
#define EFFICIENCY_RANGE (1 << 12)

static const struct VirtCapSettings kBQ25570Settings = {
  .upper_threshold_voltage = 3500,
  .lower_threshold_voltage = 3200,
  .sample_period_us = 10,
  .capacitance_uf = 1000,
  .max_cap_voltage = 4200,
  .min_cap_voltage = 0,
  .init_cap_voltage = 3200,
  .dc_output_voltage = 2300,
  .leakage_current = 9,
  .discretize = 5695,
  .output_cap_uf = 10,
  .lookup_input_efficiency = {
    {2621,3276,3440,3522,3522,3522,3522,3563,3604,},
    {3624,3686,3727,3768,3768,3768,3788,3788,3788,},
    {3788,3788,3788,3788,3788,3809,3809,3788,3727,},
    {3706,3706,3645,3604,3481,3481,3481,3481,3481,},
  },
  .lookup_output_efficiency = {
    {4995,4818,4790,4762,4735,4735,4708,4708,4681,},
    {4681,4681,4681,4654,4654,4654,4654,4654,4602,},
    {4551,4551,4602,4708,4708,4708,4708,4654,4654,},
    {4602,4654,4681,4654,4602,4628,4628,4602,4602,},
  }
};

typedef void (*virtcap_nofp_callback_func_t)(uint8_t);

void virtcap_init(struct VirtCapSettings *settings_arg,
		  virtcap_nofp_callback_func_t callback_arg,
		  struct CalibrationSettings *calib);
void virtcap_update(int32_t current_measured, int32_t voltage_measured,
		    int32_t input_current, int32_t input_voltage);
int32_t voltage_mv_to_logic(int32_t voltage);
int32_t current_ua_to_logic(int32_t current);
int32_t current_ma_to_logic(int32_t current);

int32_t lookup(int32_t table[][9], int32_t current);
void lookup_init();