#include "virtcap.h"
#include <stdint.h>
#include <stdio.h>
#include "commons.h"
#include "gpio.h"
#include "hw_config.h"

/* ---------------------------------------------------------------------
 * VirtCap
 *
 * input:
 *    output current: current flowing out of shepherd
 *    output voltage: output voltage of shepherd
 *    input current: current value from recorded trace
 *    input voltage: voltage value from recorded trace
 *
 * output:
 *    toggles shepherd output
 *
 * VirtCap emulates a energy harvesting supply chain storage capacitor and
 * buck/boost converter
 *
 * This code is written as part of the thesis of Boris Blokland
 * Any questions on this code can be send to borisblokland@gmail.com
 * ----------------------------------------------------------------------
 */

#define SHIFT 26

/* Values for converter efficiency lookup table */
static int32_t max_t1;
static int32_t max_t2;
static int32_t max_t3;
static int32_t max_t4;

static int32_t scale_index_t1;
static int32_t scale_index_t2;
static int32_t scale_index_t3;
static int32_t scale_index_t4;

// Output state of virtcap
uint8_t virtcap_output_state = FALSE;

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
	.adc_load_voltage_gain =
		(int32_t)(1 / (1.25 * 4.096) * ((1 << 18) - 1)),
	.adc_load_voltage_offset = 0,
};

void virtcap_init(struct VirtCapSettings *s_arg,
		  virtcap_nofp_callback_func_t callback_arg,
		  struct CalibrationSettings *calib)
{
	s = *s_arg;
	callback = callback_arg;
	cs = *calib;

	calib->adc_load_current_gain =
		(int32_t)(2.0 * 50.25 / (0.625 * 4.096) * ((1 << 17) - 1)),
	calib->adc_load_current_offset = -(1 << 17),
	calib->adc_load_voltage_gain =
		(int32_t)(1 / (1.25 * 4.096) * ((1 << 18) - 1)),
	calib->adc_load_voltage_offset = 0,

	*s_arg = kBQ25570Settings;

	// convert voltages and currents to logic values
	s.upper_threshold_voltage =
		voltage_mv_to_logic(s_arg->upper_threshold_voltage);
	s.lower_threshold_voltage =
		voltage_mv_to_logic(s_arg->lower_threshold_voltage);
	s.max_cap_voltage = voltage_mv_to_logic(s_arg->max_cap_voltage);
	s.min_cap_voltage = voltage_mv_to_logic(s_arg->min_cap_voltage);
	s.init_cap_voltage = voltage_mv_to_logic(s_arg->init_cap_voltage);
	s.dc_output_voltage = voltage_mv_to_logic(s_arg->dc_output_voltage);
	s.leakage_current = current_ua_to_logic(s_arg->leakage_current);

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
	harvest_multiplier = (s.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) /
			     (cs.adc_load_current_gain /
			      cs.adc_load_voltage_gain * s.capacitance_uf);

	avg_cap_voltage =
		(s.upper_threshold_voltage + s.lower_threshold_voltage) / 2;
	output_multiplier =
		s.dc_output_voltage / (avg_cap_voltage >> SHIFT_VOLT);

	lookup_init();
}

void virtcap_update(int32_t output_current, int32_t output_voltage,
		    int32_t input_current, int32_t input_voltage)
{
	int32_t input_efficiency;
	int32_t output_efficiency;

	output_efficiency = lookup(s.lookup_output_efficiency, output_current);
	input_efficiency = lookup(s.lookup_input_efficiency, input_current);

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
		if (is_outputting &&
		    (new_cap_voltage < s.lower_threshold_voltage)) {
			is_outputting = FALSE; // we fall under our threshold
			callback(FALSE);
		} else if (!is_outputting &&
			   (new_cap_voltage > s.upper_threshold_voltage)) {
			is_outputting =
				TRUE; // we have enough voltage to switch on again
			callback(TRUE);
			new_cap_voltage = (new_cap_voltage >> 10) *
					  outputcap_scale_factor;
		}
	}

	cap_voltage = new_cap_voltage;
}

uint32_t SquareRootRounded(uint32_t a_nInput)
{
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

int32_t voltage_mv_to_logic(int32_t voltage)
{
	/* Compenesate for adc gain and offset, division for mv is split to optimize
   * accuracy */
	int32_t logic_voltage = voltage * (cs.adc_load_voltage_gain / 100) / 10;
	logic_voltage += cs.adc_load_voltage_offset;
	return logic_voltage << SHIFT_VOLT;
}

int32_t current_ua_to_logic(int32_t current)
{
	/* Compensate for adc gain and offset, division for ua is split to optimize
   * accuracy */
	int32_t logic_current =
		current * (cs.adc_load_current_gain / 1000) / 1000;
	/* Add 2^17 because current is defined around zero, not 2^17 */
	logic_current += cs.adc_load_current_offset + (1 << 17);
	return logic_current;
}

void lookup_init()
{
	max_t1 = current_ua_to_logic(0.1 * 1e3);
	max_t2 = current_ua_to_logic(1 * 1e3);
	max_t3 = current_ua_to_logic(10 * 1e3);
	max_t4 = current_ua_to_logic(100 * 1e3);

	scale_index_t1 = 9 * (1 << SHIFT) / max_t1;
	scale_index_t2 = 9 * (1 << SHIFT) / max_t2;
	scale_index_t3 = 9 * (1 << SHIFT) / max_t3;
	scale_index_t4 = 9 * (1 << SHIFT) / max_t4;
}

/*
 * The lookup table returns the efficiency corresponding to the given
 * input current. Lookup table is divided into 4 sections, which each have
 * a different x-axis scale. First is determined in which section the current
 * points to. Then the current is scaled to a value between 0--9, which is used
 * as index in the lookup table.
 *
 * Figure 4: 'Charger Efficiency vs Input Current' in the datatsheet on
 * https://www.ti.com/lit/ds/symlink/bq25570.pdf shows how those 4 sections are
 * defined.
 */
int32_t lookup(int32_t table[][9], int32_t current)
{
	if (current < max_t1) {
		int32_t index = current * scale_index_t1 >> SHIFT;
		return table[0][index];
	} else if (current < max_t2) {
		int32_t index = current * scale_index_t2 >> SHIFT;
		return table[1][index];
	} else if (current < max_t3) {
		int32_t index = current * scale_index_t3 >> SHIFT;
		return table[2][index];
	} else if (current < max_t4) {
		int32_t index = current * scale_index_t4 >> SHIFT;
		return table[3][index];
	} else {
		// Should never get here
		return table[3][8];
	}
}

void set_output(uint8_t value)
{
	virtcap_output_state = value;

	if (value) {
		_GPIO_ON(DEBUG_P1);
	} else {
		_GPIO_OFF(DEBUG_P1);
	}
}
