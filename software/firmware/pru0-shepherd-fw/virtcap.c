#include <stdint.h>
#include <stdio.h>
#include "virtcap.h"
#include "commons.h"

// Derived constants
static int32_t harvest_multiplier;
static uint32_t output_multiplier;
static uint32_t outputcap_scale_factor;
static uint32_t avg_cap_voltage;

// Working vars
static uint32_t cap_voltage;
static uint8_t is_outputting;
static uint8_t is_enabled;
static uint32_t discretize_cntr;

uint32_t SquareRootRounded(uint32_t a_nInput);

// Global vars to access in update function
static VirtCapNoFpSettings settings;
static virtcap_nofp_callback_func_t callback;
static struct CalibrationSettings calibration_settings;

int32_t virtcap_init(VirtCapNoFpSettings settings_arg, virtcap_nofp_callback_func_t callback_arg, struct CalibrationSettings calib)
{
  settings = settings_arg;
  callback = callback_arg;
  calibration_settings = calib;
  
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

  // Calculate harvest multiplier
  harvest_multiplier = (settings.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) / (100 * settings.capacitance_uF);
  
  avg_cap_voltage = ((settings.upper_threshold_voltage + settings.lower_threshold_voltage) / 2);
  output_multiplier = ((settings.kDcoutputVoltage >> SHIFT_VOLT) * settings.kConverterEfficiency) / (avg_cap_voltage >> SHIFT_VOLT);

  #if 0
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
  print("input_current: %d, settings.kLeakageCurrent: %u, cap_voltage: %u\n", 
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
  /* Compenesate for adc gain and offset, division for mv is split to optimize accuracy */
  uint32_t logic_voltage = voltage * (calibration_settings.adc_load_voltage_gain / 100) / 10;
  logic_voltage += calibration_settings.adc_load_voltage_offset;
  return logic_voltage;
}

uint32_t current_ua_to_logic (uint32_t current)
{
  /* Compensate for adc gain and offset, division for ua is split to optimize accuracy */
  uint32_t logic_current = current * (calibration_settings.adc_load_current_gain / 1000) / 1000;
  /* Add 2^17 because current is defined around zero, not 2^17 */
  logic_current += calibration_settings.adc_load_current_offset + (2 >> 17);
  return logic_current;
}

uint32_t current_ma_to_logic (uint32_t current)
{
  uint32_t logic_current = current * calibration_settings.adc_load_current_gain / 1000;
  /* Add 2^17 because current is defined around zero, not 2^17 */
  logic_current += calibration_settings.adc_load_current_offset + (2 >> 17);
  return logic_current;
}
