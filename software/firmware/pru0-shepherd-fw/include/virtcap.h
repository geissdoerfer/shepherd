#pragma once

#include <stdint.h>
#include "commons.h"

#define TRUE  1
#define FALSE 0

#define SHIFT_VOLT 12

typedef struct VirtCapNoFpSettings
{
  int32_t upper_threshold_voltage;
  int32_t lower_threshold_voltage;
  uint16_t sample_period_us;
  uint16_t capacitance_uF;
  int32_t kMaxCapVoltage;
  int32_t kMinCapVoltage;
  int32_t kInitCapVoltage;
  int32_t kDcoutputVoltage;
  int32_t kLeakageCurrent;
  int32_t kHarvesterEfficiency;
  int32_t kConverterEfficiency;
  int32_t kOnTimeLeakageCurrent;
  const char* kFilename;
  int kScaleDacOut;
  int32_t kDiscretize;
  uint16_t kOutputCap_uF;
} VirtCapNoFpSettings;

static const VirtCapNoFpSettings kRealBQ25570Settings = 
{
  .upper_threshold_voltage = 3500,
  .lower_threshold_voltage = 3200,
  .sample_period_us = 10,   
  .capacitance_uF = 520,    
  .kMaxCapVoltage = 4200,
  .kMinCapVoltage = 0,    
  .kInitCapVoltage = 3200,
  .kDcoutputVoltage = 2210,
  .kLeakageCurrent = 4,
  .kHarvesterEfficiency = 0.9 * 8192,   
  .kConverterEfficiency = 0.86 * 8192,
  .kOnTimeLeakageCurrent = 12,
  .kFilename = "solar3.bin",
  .kScaleDacOut = 2,
  .kDiscretize = 5695, //5695,
  .kOutputCap_uF = 10,
};

typedef void (*virtcap_nofp_callback_func_t)(uint8_t);

int32_t virtcap_init(VirtCapNoFpSettings settings_arg, virtcap_nofp_callback_func_t callback_arg, struct CalibrationSettings calib);
int32_t virtcap_update(int32_t current_measured, int32_t voltage_measured, int32_t input_current, int32_t input_voltage, int32_t efficiency);
int32_t voltage_mv_to_logic (int32_t voltage);
int32_t current_ua_to_logic (int32_t current);
int32_t current_ma_to_logic (int32_t current);
