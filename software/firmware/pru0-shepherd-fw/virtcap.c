#include <stdint.h>
#include <stdio.h>
#include "virtcap.h"
#include "commons.h"

#define DEBUG_PRINT 1
#define debug_print(...) \
            do { if (DEBUG_PRINT) fprintf(stderr, __VA_ARGS__); } while (0)

// Derived constants
int32_t harvest_multiplier;
uint32_t output_multiplier;
uint32_t kScaleInput;
uint32_t outputcap_scale_factor;
uint32_t avg_cap_voltage;

// Working vars
uint32_t cap_voltage;
uint8_t is_outputting;
uint8_t is_enabled;
uint32_t discretize_cntr;

uint32_t SquareRootRounded(uint32_t a_nInput);

// Global vars to access in update function
VirtCapNoFpSettings settings;
virtcap_nofp_callback_func_t callback;
struct CalibrationSettings calibration_settings;

int32_t virtcap_init(VirtCapNoFpSettings settings_arg, virtcap_nofp_callback_func_t callback_arg, struct CalibrationSettings calib)
{
  settings = settings_arg;
  callback = callback_arg;

  calibration_settings = calib;
  
  // Calculate harvest multiplier

  // convert voltages and currents to logic values
  settings.upper_threshold_voltage = voltage_mv_to_logic(settings.upper_threshold_voltage);
  settings.lower_threshold_voltage = voltage_mv_to_logic(settings.lower_threshold_voltage);
  settings.kMaxCapVoltage = voltage_mv_to_logic(settings.kMaxCapVoltage);
  settings.kMinCapVoltage = voltage_mv_to_logic(settings.kMinCapVoltage);
  settings.kInitCapVoltage = voltage_mv_to_logic(settings.kInitCapVoltage);
  settings.kDcoutputVoltage = voltage_mv_to_logic(settings.kDcoutputVoltage);
  settings.kLeakageCurrent = current_ua_to_logic(settings.kLeakageCurrent);
  settings.kOnTimeLeakageCurrent = current_ua_to_logic(settings.kOnTimeLeakageCurrent);

  uint32_t outputcap_scale_factor_pre_sqrt = (settings.capacitance_uF - settings.kOutputCap_uF) * 1024 * 1024 / settings.capacitance_uF;

  outputcap_scale_factor = SquareRootRounded(outputcap_scale_factor_pre_sqrt);

  // Initialize vars
  cap_voltage = settings.kInitCapVoltage;
  is_outputting = FALSE;
  is_enabled = FALSE;
  discretize_cntr = 0;

  // 100.5 * (1 << 17 - 1) * (1 << 18 - 1) / (4.096 * 8.192) / 1e6;
  kScaleInput = 102911;

  harvest_multiplier = (settings.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) / (100 * settings.capacitance_uF);
  
  avg_cap_voltage = ((settings.upper_threshold_voltage + settings.lower_threshold_voltage) / 2);
  output_multiplier = ((settings.kDcoutputVoltage >> SHIFT_VOLT) * settings.kConverterEfficiency) / (avg_cap_voltage >> SHIFT_VOLT);

  #if 0
  // printf("kScaleInput: %d\n", kScaleInput);
  printf("settings.kDcoutputVoltage: %d, settings.kConverterEfficiency: %d, avg_cap_voltage: %d, output_multiplier: %d\n", 
        settings.kDcoutputVoltage, settings.kConverterEfficiency, avg_cap_voltage, output_multiplier);
  #endif 

  return output_multiplier;
}

int32_t virtcap_update(int32_t current_measured, uint32_t voltage_measured, int32_t input_current, uint32_t input_voltage, uint32_t efficiency)
{
  // x * 1000 * efficiency / 100
  input_current = input_current * input_voltage / (cap_voltage >> SHIFT_VOLT) * efficiency >> SHIFT_VOLT;

  input_current -= (settings.kLeakageCurrent); // compensate for leakage current
  
  // compensate output current for higher voltage than input + boost converter efficiency
  if (!is_outputting)
  {
    current_measured = 0; // force current read to zero while not outputting (to ignore noisy current reading)
  }

  // int32_t output_current = current_measured * (settings.kDcoutputVoltage >> SHIFT_VOLT) / (avg_cap_voltage >> SHIFT_VOLT) * settings.kConverterEfficiency >> SHIFT_VOLT; // + kOnTimeLeakageCurrent;
  int32_t output_current = current_measured * output_multiplier >> SHIFT_VOLT;

  uint32_t new_cap_voltage = cap_voltage + ((input_current - output_current) * harvest_multiplier >> SHIFT_VOLT);
  
  /**
   * dV = dI * dt / C
   * dV' * 3.3 / 4095 / 1000 / 512 = dI' * 0.033 * dt / (C * 4095 * 1000)
   * dV' = (dI' * 0.033 * dt * 512) / (3.3 * C)
   * dV' = (dI' * dt * 512) / (100 * C)
   * dV' = (dI' * 5120) / (100 * C)
   */

  #if (SINGLE_ITERATION == 1)
  debug_print("input_current: %d, settings.kLeakageCurrent: %u, cap_voltage: %u\n", 
              input_current, settings.kLeakageCurrent, cap_voltage);
  #endif

  // TODO remove debug
  #if (PRINT_INTERMEDIATE == 1)
  static uint16_t cntr;
  if (cntr++ % 100000 == 0)
  {
    printf("input_current: %d, current_measured: %u, voltage_measured: %u, cap_voltage: %u\n",
            input_current, current_measured, voltage_measured, cap_voltage); 
    printf("new_cap_voltage: %u, is_outputting: %d, output_current: %d \n", 
          new_cap_voltage, is_outputting, output_current);
  } 
  #endif
  
  // Make sure the voltage does not go beyond it's boundaries
  if (new_cap_voltage >= settings.kMaxCapVoltage)
  {
    new_cap_voltage = settings.kMaxCapVoltage;
  }
  else if (new_cap_voltage < settings.kMinCapVoltage)
  {
    new_cap_voltage = settings.kMinCapVoltage;
  }

  // only update output every 'kDiscretize' time
  discretize_cntr++;
  if (discretize_cntr >= settings.kDiscretize)
  {
    discretize_cntr = 0;

    // determine whether we should be in a new state
    if (is_outputting && (new_cap_voltage < settings.lower_threshold_voltage))
    {
      is_outputting = FALSE; // we fall under our threshold
      callback(FALSE);
    }
    else if (!is_outputting && (new_cap_voltage > settings.upper_threshold_voltage))
    {
      is_outputting = TRUE; // we have enough voltage to switch on again
      callback(TRUE);
      new_cap_voltage = (new_cap_voltage >> 10) * outputcap_scale_factor;
    }

  }

  cap_voltage = new_cap_voltage;

  return output_current;
}

uint32_t SquareRootRounded(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type


    // "one" starts at the highest power of four <= than the argument.
    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    /* Do arithmetic rounding to nearest integer */
    if (op > res)
    {
        res++;
    }

    return res;
}

uint32_t voltage_mv_to_logic (uint32_t voltage)
{
  // voltage * (1 << 18 - 1) / 8.192 / 1.25 / 1000;
  return voltage * calibration_settings.
}

uint32_t current_ua_to_logic (uint32_t current)
{
  // current * 100.5 * (1 << 17 - 1) / 4.096 / 0.625 / 1000;
  return current * 3216 / 1000;
}

uint32_t current_ma_to_logic (uint32_t current)
{
  // current * 100.5 * (1 << 17 - 1) / 4.096 / 0.625;
  return current * 3216;
}