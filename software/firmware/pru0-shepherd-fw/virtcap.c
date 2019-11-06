#include "virtcap.h"
#include <stdint.h>
#include <stdio.h>
#include "commons.h"

// Derived constants
static int32_t harvest_multiplier;
static int32_t output_multiplier;
static int32_t outputcap_scale_factor;
static int32_t avg_cap_voltage;

// Working vars
static int32_t cap_voltage;
static uint8_t is_outputting;
static int32_t discretize_cntr;

uint32_t SquareRootRounded(uint32_t a_nInput);

// Global vars to access in update function
static VirtCapNoFpSettings s;
static virtcap_nofp_callback_func_t callback;
static struct CalibrationSettings cs;

#define FIXED_VALUES 1


int32_t virtcap_init(VirtCapNoFpSettings settings_arg,
                     virtcap_nofp_callback_func_t callback_arg,
                     struct CalibrationSettings calib) {
  s = settings_arg;
  callback = callback_arg;
  cs = calib;

  #if (FIXED_VALUES == 1)
  s.upper_threshold_voltage = 3.5 * 1e3;
  s.lower_threshold_voltage = 3.2 * 1e3;
  s.capacitance_uF = 1000;    
  s.sample_period_us = 10;   
  s.kMaxCapVoltage = 4.2 * 1e3;
  s.kMinCapVoltage = 0;    
  s.kInitCapVoltage = 3.2 * 1e3;
  s.kDcoutputVoltage = 2.23 * 1e3;
  s.kLeakageCurrent = 4;
  s.kHarvesterEfficiency = 0.90 * 8192;   
  s.kConverterEfficiency = 0.86 * 8192;
  s.kOnTimeLeakageCurrent = 12;
  s.kFilename = "solv3_10.bin";
  s.kScaleDacOut = 2;
  s.kDiscretize = 5695;
  s.kOutputCap_uF = 10;

  cs.adc_load_current_gain = (int32_t) (2.0 * 50.25 / (0.625 * 4.096) * ((1 << 17) - 1));
  cs.adc_load_current_offset = - (1 << 17);
  cs.adc_load_voltage_gain = (int32_t) (1 / (1.25 * 4.096) * ((1 << 18) - 1));
  cs.adc_load_voltage_offset = 0;
  #endif

  // convert voltages and currents to logic values
  s.upper_threshold_voltage = voltage_mv_to_logic(s.upper_threshold_voltage);
  s.lower_threshold_voltage = voltage_mv_to_logic(s.lower_threshold_voltage);
  s.kMaxCapVoltage = voltage_mv_to_logic(s.kMaxCapVoltage);
  s.kMinCapVoltage = voltage_mv_to_logic(s.kMinCapVoltage);
  s.kInitCapVoltage = voltage_mv_to_logic(s.kInitCapVoltage);
  s.kDcoutputVoltage = voltage_mv_to_logic(s.kDcoutputVoltage);
  s.kLeakageCurrent = current_ua_to_logic(s.kLeakageCurrent);
  s.kOnTimeLeakageCurrent = current_ua_to_logic(s.kOnTimeLeakageCurrent);

  /* Calculate how much output cap should be discharged when turning on, based
   * on the storage capacitor and output capacitor size */
  int32_t scale =
      ((s.capacitance_uF - s.kOutputCap_uF) << 20) / s.capacitance_uF;
  outputcap_scale_factor = SquareRootRounded(scale);

  // Initialize vars
  cap_voltage = s.kInitCapVoltage;
  is_outputting = FALSE;
  discretize_cntr = 0;

  // Calculate harvest multiplier
  harvest_multiplier =
      (s.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) /
      (cs.adc_load_current_gain / cs.adc_load_voltage_gain * s.capacitance_uF);

  avg_cap_voltage = (s.upper_threshold_voltage + s.lower_threshold_voltage) / 2;
  output_multiplier =
      ((s.kDcoutputVoltage >> SHIFT_VOLT) * s.kConverterEfficiency) /
      (avg_cap_voltage >> SHIFT_VOLT);

#if (SINGLE_ITERATION == 1)
  printf(
      "current_gain: %d, current_offset: %d, voltage_gain: %d, voltage_offset: "
      "%d\n",
      calib.adc_load_current_gain, calib.adc_load_current_offset,
      calib.adc_load_voltage_gain, calib.adc_load_voltage_offset);

  printf(
      "s.kDcoutputVoltage: %d, s.kConverterEfficiency: %d, avg_cap_voltage: "
      "%d, output_multiplier: %d\n",
      s.kDcoutputVoltage, s.kConverterEfficiency, avg_cap_voltage,
      output_multiplier);
#endif

  return output_multiplier;
}


int32_t virtcap_update(int32_t output_current, int32_t output_voltage,
                       int32_t input_current, int32_t input_voltage,
                       int32_t efficiency) {
#if (FIXED_VALUES == 1)
  output_current = 24006;
  output_voltage = 113953;
  input_current = 7717;
  input_voltage = 76650;
  efficiency = 0.9 * 8192;
#endif

  /* Calculate current flowing into the storage capacitor */
  int32_t input_power = input_current * input_voltage;
  int32_t input_current_cap = input_power / (cap_voltage >> SHIFT_VOLT);
  input_current_cap *= efficiency;
  input_current_cap = input_current_cap >> SHIFT_VOLT;
  input_current_cap -= s.kLeakageCurrent;

  /* Calculate current flowing out of the storage capacitor*/
  if (!is_outputting) {
    output_current = 0;
  }
  int32_t output_current_cap = output_current * output_multiplier >> SHIFT_VOLT;

  /* Calculate delta V*/
  int32_t delta_i = input_current_cap - output_current_cap;
  int32_t delta_v = delta_i * harvest_multiplier >> SHIFT_VOLT;
  int32_t new_cap_voltage = cap_voltage + delta_v;

#if (SINGLE_ITERATION == 1)
  printf(
      "input_current_cap: %d, input_current_cap: %d, s.kLeakageCurrent: %u, "
      "cap_voltage: %u\n",
      input_current, input_current_cap, s.kLeakageCurrent, cap_voltage);
#endif

// TODO remove debug
#if (PRINT_INTERMEDIATE == 1)
  static uint16_t cntr;
  if (cntr++ % 100000 == 0) {
    printf(
        "input_current_cap: %d, output_current: %u, output_voltage: %u, "
        "cap_voltage: %u\n",
        input_current_cap, output_current, output_voltage, cap_voltage);
    printf("new_cap_voltage: %u, is_outputting: %d, output_current_cap: %d \n",
           new_cap_voltage, is_outputting, output_current_cap);
  }
#endif

  // Make sure the voltage does not go beyond it's boundaries
  if (new_cap_voltage >= s.kMaxCapVoltage) {
    new_cap_voltage = s.kMaxCapVoltage;
  } else if (new_cap_voltage < s.kMinCapVoltage) {
    new_cap_voltage = s.kMinCapVoltage;
  }

  // only update output every 'kDiscretize' time
  discretize_cntr++;
  if (discretize_cntr >= s.kDiscretize) {
    discretize_cntr = 0;

    // determine whether we should be in a new state
    if (is_outputting && (new_cap_voltage < s.lower_threshold_voltage)) {
      is_outputting = FALSE;  // we fall under our threshold
      callback(FALSE);
    } else if (!is_outputting &&
               (new_cap_voltage > s.upper_threshold_voltage)) {
      is_outputting = TRUE;  // we have enough voltage to switch on again
      callback(TRUE);
      new_cap_voltage = (new_cap_voltage >> 10) * outputcap_scale_factor;
    }
  }

  cap_voltage = new_cap_voltage;

  return new_cap_voltage;
}

uint32_t SquareRootRounded(uint32_t a_nInput) {
  uint32_t op = a_nInput;
  uint32_t res = 0;
  uint32_t one = 1uL << 30;

  while (one > op) {
    one >>= 2;
  }

  while (one != 0) {
    if (op >= res + one) {
      op = op - (res + one);
      res = res + 2 * one;
    }
    res >>= 1;
    one >>= 2;
  }

  if (op > res) {
    res++;
  }

  return res;
}

int32_t voltage_mv_to_logic(int32_t voltage) {
  /* Compenesate for adc gain and offset, division for mv is split to optimize
   * accuracy */
  int32_t logic_voltage = voltage * (cs.adc_load_voltage_gain / 100) / 10;
  logic_voltage += cs.adc_load_voltage_offset;
  return logic_voltage << SHIFT_VOLT;
}

int32_t current_ua_to_logic(int32_t current) {
  /* Compensate for adc gain and offset, division for ua is split to optimize
   * accuracy */
  int32_t logic_current = current * (cs.adc_load_current_gain / 1000) / 1000;
  /* Add 2^17 because current is defined around zero, not 2^17 */
  logic_current += cs.adc_load_current_offset + (1 << 17);
  return logic_current;
}

int32_t current_ma_to_logic(int32_t current) {
  int32_t logic_current = current * cs.adc_load_current_gain / 1000;
  /* Add 2^17 because current is defined around zero, not 2^17 */
  logic_current += cs.adc_load_current_offset + (1 << 17);
  return logic_current;
}
