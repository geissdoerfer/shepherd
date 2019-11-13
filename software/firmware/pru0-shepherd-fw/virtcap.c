#include <stdint.h>
#include <stdio.h>
#include "commons.h"
#include "virtcap.h"

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
struct VirtCapSettings s;
virtcap_nofp_callback_func_t callback;
struct CalibrationSettings cs = {
    .adc_load_current_gain =
        (int32_t)(2.0 * 50.25 / (0.625 * 4.096) * ((1 << 17) - 1)),
    .adc_load_current_offset = -(1 << 17),
    .adc_load_voltage_gain = (int32_t)(1 / (1.25 * 4.096) * ((1 << 18) - 1)),
    .adc_load_voltage_offset = 0,
};

#define FIXED_SETTINGS 0
#define FIXED_CALIBRATION 0
#define FIXED_INPUT 0
#define FIXED_EFFICIENCY 0

int32_t virtcap_init(const struct VirtCapSettings* s_arg,
                     virtcap_nofp_callback_func_t callback_arg,
                     const struct CalibrationSettings* calib, int32_t dbg[]) {
  s = *s_arg;
  callback = callback_arg;
  cs = *calib;

#if (FIXED_INPUT == 1 || FIXED_SETTINGS == 1)
  s.upper_threshold_voltage = 3.5 * 1e3;
  s.lower_threshold_voltage = 3.2 * 1e3;
  s.capacitance_uf = 137;
  s.sample_period_us = 10;
  s.max_cap_voltage = 4.2 * 1e3;
  s.min_cap_voltage = 0;
  s.init_cap_voltage = 3.2 * 1e3;
  s.dc_output_voltage = 2.23 * 1e3;
  s.leakage_current = 4;
  s.discretize = 5695;
  s.output_cap_uf = 10;
#endif

  // convert voltages and currents to logic values
  s.upper_threshold_voltage = voltage_mv_to_logic(s_arg->upper_threshold_voltage);
  s.lower_threshold_voltage = voltage_mv_to_logic(s_arg->lower_threshold_voltage);
  s.max_cap_voltage = voltage_mv_to_logic(s_arg->max_cap_voltage);
  s.min_cap_voltage = voltage_mv_to_logic(s_arg->min_cap_voltage);
  s.init_cap_voltage = voltage_mv_to_logic(s_arg->init_cap_voltage);
  s.dc_output_voltage = voltage_mv_to_logic(s_arg->dc_output_voltage);
  s.leakage_current = current_ua_to_logic(s_arg->leakage_current);

  #if (FIXED_CALIBRATION == 1)
  cs.adc_load_current_gain = (int32_t) (2.0 * 50.25 / (0.625 * 4.096) * ((1 << 17) - 1)),
  cs.adc_load_current_offset = - (1 << 17),
  cs.adc_load_voltage_gain = (int32_t) (1 / (1.25 * 4.096) * ((1 << 18) - 1)),
  cs.adc_load_voltage_offset = -2,
  #endif

  #if 1
  dbg[4] = cs.adc_load_current_gain;
  dbg[5] = cs.adc_load_current_offset;
  dbg[6] = cs.adc_load_voltage_gain;
  dbg[7] = cs.adc_load_voltage_offset;
  #else
  dbg[4] = s.discretize;
  dbg[5] = s.output_cap_uf;
  dbg[6] = s.max_cap_voltage;
  dbg[7] = s.min_cap_voltage;
  #endif

  /* Calculate how much output cap should be discharged when turning on, based
   * on the storage capacitor and output capacitor size */
  int32_t scale =
      ((s.capacitance_uf - s.output_cap_uf) << 20) / s.capacitance_uf;
  outputcap_scale_factor = SquareRootRounded(scale);

  // Initialize vars
  cap_voltage = s.init_cap_voltage;
  is_outputting = FALSE;
  discretize_cntr = 0;

  // Calculate harvest multiplier
  harvest_multiplier =
      (s.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) /
      (cs.adc_load_current_gain / cs.adc_load_voltage_gain * s.capacitance_uf);

  avg_cap_voltage = (s.upper_threshold_voltage + s.lower_threshold_voltage) / 2;
  output_multiplier = s.dc_output_voltage / (avg_cap_voltage >> SHIFT_VOLT);

  lookup_init(dbg);

#if (SINGLE_ITERATION == 1)
  printf(
      "current_gain: %d, current_offset: %d, voltage_gain: %d, voltage_offset: "
      "%d\n",
      calib.adc_load_current_gain, calib.adc_load_current_offset,
      calib.adc_load_voltage_gain, calib.adc_load_voltage_offset);

  printf(
      "s.dc_output_voltage: %d, s.kConverterEfficiency: %d, avg_cap_voltage: "
      "%d, output_multiplier: %d\n",
      s.dc_output_voltage, s.kConverterEfficiency, avg_cap_voltage,
      output_multiplier);
#endif

  return output_multiplier;
}

int32_t virtcap_update(int32_t output_current, int32_t output_voltage,
                       int32_t input_current, int32_t input_voltage, int32_t dbg[]) {

  int32_t input_efficiency;
  int32_t output_efficiency;


#if (FIXED_INPUT == 1)
  output_current = 24006;
  output_voltage = 113953;
  input_current = 7717;
  input_voltage = 76650;
#endif

  #if (FIXED_EFFICIENCY == 1)
  input_efficiency = 0.90 * EFFICIENCY_RANGE;
  output_efficiency = (1 / 0.86) * EFFICIENCY_RANGE;
  #else
  output_efficiency = lookup(s.lookup_output_efficiency, output_current, dbg);
  input_efficiency = lookup(s.lookup_input_efficiency, input_current, dbg);
  #endif 

  /* Calculate current (cin) flowing into the storage capacitor */
  int32_t input_power = input_current * input_voltage;
  int32_t cin = input_power / (cap_voltage >> SHIFT_VOLT);
  cin *= input_efficiency;
  cin = cin >> SHIFT_VOLT;

  /* Calculate current (cout) flowing out of the storage capacitor*/
  if (!is_outputting) {
    output_current = 0;
  }
  int32_t cout = output_current * output_multiplier >> SHIFT_VOLT;
  cout *= output_efficiency;
  cout = cout >> SHIFT_VOLT;
  cout += s.leakage_current;

  /* Calculate delta V*/
  int32_t delta_i = cin - cout;
  int32_t delta_v = delta_i * harvest_multiplier >> SHIFT_VOLT;
  int32_t new_cap_voltage = cap_voltage + delta_v;

#if (SINGLE_ITERATION == 1)
  printf(
      "cin: %d, cin: %d, s.leakage_current: %u, "
      "cap_voltage: %u\n",
      input_current, cin, s.leakage_current, cap_voltage);
#endif

// TODO remove debug
#if (PRINT_INTERMEDIATE == 1)
  static uint16_t cntr;
  if (cntr++ % 100000 == 0) {
    printf(
        "output_current: %d, output_voltage: %d, input_current: %d, "
        "input_voltage: %d\n",
        output_current, output_voltage, input_current, input_voltage);
  }
#endif

  // Make sure the voltage does not go beyond it's boundaries
  if (new_cap_voltage >= s.max_cap_voltage) {
    new_cap_voltage = s.max_cap_voltage;
  } else if (new_cap_voltage < s.min_cap_voltage) {
    new_cap_voltage = s.min_cap_voltage;
  }

  // only update output every 'discretize' time
  discretize_cntr++;
  if (discretize_cntr >= s.discretize) {
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

  return cap_voltage;
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
