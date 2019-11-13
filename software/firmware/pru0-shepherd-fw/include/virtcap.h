#pragma once

#include <stdint.h>
#include "commons.h"
#include "lookup_table.h"

#define TRUE  1
#define FALSE 0

#define SHIFT_VOLT 12
#define EFFICIENCY_RANGE (1 << 12)

typedef struct VirtCapNoFpSettings {
  int32_t upper_threshold_voltage;
  int32_t lower_threshold_voltage;
  uint16_t sample_period_us;
  uint16_t capacitance_uf;
  int32_t kMaxCapVoltage;
  int32_t kMinCapVoltage;
  int32_t kInitCapVoltage;
  int32_t kDcoutputVoltage;
  int32_t kLeakageCurrent;
  int32_t kDiscretize;
  uint16_t kOutputCap_uf;
  int16_t kLookupInputEfficiency[4][9];
  int16_t kLookupOutputEfficiency[4][9];
} VirtCapNoFpSettings;

static const VirtCapNoFpSettings kBQ25570Settings = 
{
  .upper_threshold_voltage = 3500,
  .lower_threshold_voltage = 3200,
  .sample_period_us = 10,   
  .capacitance_uf = 1000,    
  .kMaxCapVoltage = 4200,
  .kMinCapVoltage = 0,    
  .kInitCapVoltage = 3200,
  .kDcoutputVoltage = 2300,
  .kLeakageCurrent = 9,
  .kDiscretize = 5695,
  .kOutputCap_uf = 10,
  .kLookupInputEfficiency = {
    {2621,3276,3440,3522,3522,3522,3522,3563,3604,},
    {3624,3686,3727,3768,3768,3768,3788,3788,3788,},
    {3788,3788,3788,3788,3788,3809,3809,3788,3727,},
    {3706,3706,3645,3604,3481,3481,3481,3481,3481,},
  },
  .kLookupOutputEfficiency = {
    {4995,4818,4790,4762,4735,4735,4708,4708,4681,},
    {4681,4681,4681,4654,4654,4654,4654,4654,4602,},
    {4551,4551,4602,4708,4708,4708,4708,4654,4654,},
    {4602,4654,4681,4654,4602,4628,4628,4602,4602,},
  }
};

typedef void (*virtcap_nofp_callback_func_t)(uint8_t);

int32_t virtcap_init(const VirtCapNoFpSettings* settings_arg, virtcap_nofp_callback_func_t callback_arg, const struct CalibrationSettings* calib, int32_t dbg[]);
int32_t virtcap_update(int32_t current_measured, int32_t voltage_measured, int32_t input_current, int32_t input_voltage, int32_t dbg[]);
int32_t voltage_mv_to_logic (int32_t voltage);
int32_t current_ua_to_logic (int32_t current);
int32_t current_ma_to_logic (int32_t current);
