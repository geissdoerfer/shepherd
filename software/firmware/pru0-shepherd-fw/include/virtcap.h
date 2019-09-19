#pragma once

#include <stdint.h>

#define TRUE  1
#define FALSE 0

#define SHIFT_VOLT 13

typedef struct VirtCapNoFpSettings
{
  uint32_t upper_threshold_voltage;
  uint32_t lower_threshold_voltage;
  uint16_t sample_period_us;
  uint16_t capacitance_uF;
  uint32_t kMaxCapVoltage;
  uint32_t kMinCapVoltage;
  uint32_t kInitCapVoltage;
  uint32_t kDcoutputVoltage;
  uint32_t kLeakageCurrent;
  uint32_t kHarvesterEfficiency;
  uint32_t kConverterEfficiency;
  uint32_t kOnTimeLeakageCurrent;
  const char* kFilename;
  int kScaleDacOut;
  uint32_t kDiscretize;
  uint16_t kOutputCap_uF;
} VirtCapNoFpSettings;

static const VirtCapNoFpSettings kRealBQ25570Settings = 
{
  .upper_threshold_voltage = 3500,
  .lower_threshold_voltage = 3200,
  .sample_period_us = 10,   
  .capacitance_uF = 1000,    
  .kMaxCapVoltage = 4200,
  .kMinCapVoltage = 0,    
  .kInitCapVoltage = 3200,
  .kDcoutputVoltage = 2210,
  .kLeakageCurrent = 9,
  .kHarvesterEfficiency = 0.9 * 8192,   
  .kConverterEfficiency = (1 / 0.9) * 8192,
  .kOnTimeLeakageCurrent = 12,
  .kFilename = "solar3.bin",
  .kScaleDacOut = 2,
  .kDiscretize = 5695, //5695,
  .kOutputCap_uF = 10,
};

typedef void (*virtcap_nofp_callback_func_t)(uint8_t);

void virtcap_init(VirtCapNoFpSettings settings, virtcap_nofp_callback_func_t callback);
void virtcap_update(int32_t current_measured, uint32_t voltage_measured, int32_t input_current, uint32_t input_voltage, uint32_t efficiency);
uint32_t voltage_mv_to_logic (uint32_t voltage);
uint32_t current_ua_to_logic (uint32_t current);
