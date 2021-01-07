#include "virtcap.h"
#include <stdint.h>
#include <stdio.h>
#include "commons.h"
#include "gpio.h"
#include "hw_config.h"
#include "stdint_fast.h"

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

#define SHIFT_LUT   26U

/* Values for converter efficiency lookup table */ // TODO: deglobalize, can't it be unsigned?
static int32_t max_t1;
static int32_t max_t2;
static int32_t max_t3;
static int32_t max_t4;

static int32_t scale_index_t1;
static int32_t scale_index_t2;
static int32_t scale_index_t3;
static int32_t scale_index_t4;

// Output state of virtcap
static bool_ft VIRTCAP_OUT_PIN_state = 0U;

// Derived constants
static int32_t harvest_multiplier;
static int32_t output_multiplier;
static int32_t outputcap_scale_factor;
static int32_t avg_cap_voltage;

// Working vars
static int32_t cap_voltage;
static bool_ft is_outputting;
static int32_t discretize_cntr;

uint32_t SquareRootRounded(uint32_t a_nInput);
static void virtcap_set_output_state(bool_ft value);

// Global vars to access in update function
static struct VirtCapSettings vcap_cfg;

#define ADC_LOAD_CURRENT_GAIN       (int32_t)(((1U << 17U) - 1) * 2.0 * 50.25 / (0.625 * 4.096))
#define ADC_LOAD_CURRENT_OFFSET     (-(1U << 17U))  // TODO: should be positive
#define ADC_LOAD_VOLTAGE_GAIN       (int32_t)(((1U << 18U) - 1) / (1.25 * 4.096))
#define ADC_LOAD_VOLTAGE_OFFSET     0

static struct CalibrationSettings cali_cfg = {
	.adc_load_current_gain =    ADC_LOAD_CURRENT_GAIN,
	.adc_load_current_offset =  ADC_LOAD_CURRENT_OFFSET,
	.adc_load_voltage_gain =    ADC_LOAD_VOLTAGE_GAIN,
	.adc_load_voltage_offset =  ADC_LOAD_VOLTAGE_OFFSET,
};

void virtcap_init(struct VirtCapSettings *const vcap_arg,
		  struct CalibrationSettings *const cali_arg)
{
	vcap_cfg = *vcap_arg; // copies content of whole struct
	cali_cfg = *cali_arg;

	GPIO_OFF(VIRTCAP_OUT_PIN);

	cali_arg->adc_load_current_gain = ADC_LOAD_CURRENT_GAIN; // TODO: why overwriting values provided by system?
	cali_arg->adc_load_current_offset = ADC_LOAD_CURRENT_OFFSET;
	cali_arg->adc_load_voltage_gain = ADC_LOAD_VOLTAGE_GAIN;
	cali_arg->adc_load_voltage_offset = ADC_LOAD_VOLTAGE_OFFSET;

	*vcap_arg = kBQ25570Settings; // TODO: weird config in 3 Steps, why overwriting values provided by system?

	// convert voltages and currents to logic values
	vcap_cfg.upper_threshold_voltage =	voltage_mv_to_logic(vcap_arg->upper_threshold_voltage);
	vcap_cfg.lower_threshold_voltage =	voltage_mv_to_logic(vcap_arg->lower_threshold_voltage);
	vcap_cfg.max_cap_voltage = voltage_mv_to_logic(vcap_arg->max_cap_voltage);
	vcap_cfg.min_cap_voltage = voltage_mv_to_logic(vcap_arg->min_cap_voltage);
	vcap_cfg.init_cap_voltage = voltage_mv_to_logic(vcap_arg->init_cap_voltage);
	vcap_cfg.dc_output_voltage = voltage_mv_to_logic(vcap_arg->dc_output_voltage);
	vcap_cfg.leakage_current = current_ua_to_logic(vcap_arg->leakage_current);

	/* Calculate how much output cap should be discharged when turning on, based
	* on the storage capacitor and output capacitor size */
	// TODO: seems wrong, even the formular mentioned in thesis, it assumes C_out gets only V_cap...
	// base: C_cap * V_cap_new^2 / 2 = C_cap * V_cap_old^2 / 2 - C_out * V_out^2 / 2
	const int32_t scale =	((vcap_cfg.capacitance_uf - vcap_cfg.output_cap_uf) << 20U) / vcap_cfg.capacitance_uf;
	outputcap_scale_factor = SquareRootRounded(scale);

	// Initialize vars
	cap_voltage = vcap_cfg.init_cap_voltage;
	is_outputting = false;
	discretize_cntr = 0;

	// Calculate harvest multiplier
	harvest_multiplier = (vcap_cfg.sample_period_us << (SHIFT_VOLT + SHIFT_VOLT)) /
			     (cali_cfg.adc_load_current_gain / cali_cfg.adc_load_voltage_gain * vcap_cfg.capacitance_uf);

	avg_cap_voltage = (vcap_cfg.upper_threshold_voltage + vcap_cfg.lower_threshold_voltage) / 2;
	output_multiplier = vcap_cfg.dc_output_voltage / (avg_cap_voltage >> SHIFT_VOLT);

	lookup_init();
	// TODO: add tests for valid ranges
}

void virtcap_update(int32_t output_current, const int32_t output_voltage,
		    const int32_t input_current, const int32_t input_voltage) // TODO: out-volt not needed here, even remove spi-read from calling fn
{
	// TODO: explain design goals and limitations... why does the code looks that way

	const int32_t output_efficiency = lookup(vcap_cfg.lookup_output_efficiency, output_current);
    	const int32_t input_efficiency = lookup(vcap_cfg.lookup_input_efficiency, input_current);

	// TODO: whole model should be transformed to unsigned, values don't change sign (except sum of dV_cap), we get more resolution, cleaner bit-shifts and safer array access
	/* Calculate current (cin) flowing into the storage capacitor */
	const int32_t input_power = input_current * input_voltage; // TODO: data could already be preprocessed by system fpu
	int32_t cin = input_power / (cap_voltage >> SHIFT_VOLT); // TODO: cin, cout are dI_in, dI_out
	cin *= input_efficiency;
	cin = cin >> SHIFT_VOLT;

	/* Calculate current (cout) flowing out of the storage capacitor*/
	if (!is_outputting) output_current = 0;

	int32_t cout = (output_current * output_multiplier) >> SHIFT_VOLT; // TODO: crude simplification here, brings error of +-5%
	cout *= output_efficiency; // TODO: efficiency should be divided for the output, LUT seems to do that, but name confuses
	cout = cout >> SHIFT_VOLT; // TODO: shift should be some kind of DIV4096() or the real thing, it will get optimized (probably)
	cout += vcap_cfg.leakage_current; // TODO: ESR could also be considered

	/* Calculate delta V*/
	const int32_t delta_i = cin - cout;
	const int32_t delta_v = (delta_i * harvest_multiplier) >> SHIFT_VOLT; // TODO: looks wrong, harvest mult is specific for V*A ADC-Gains, but for OUT we have no Volt, and for leakage neither
	int32_t new_cap_voltage = cap_voltage + delta_v; // TODO: var can already be the original cap_voltage

	// Make sure the voltage does not go beyond it's boundaries
	if (new_cap_voltage > vcap_cfg.max_cap_voltage)         new_cap_voltage = vcap_cfg.max_cap_voltage;
	else if (new_cap_voltage < vcap_cfg.min_cap_voltage)    new_cap_voltage = vcap_cfg.min_cap_voltage;

	// TODO: there is another effect of the converter -> every 16 seconds it optimizes power-draw, is it already in the data-stream?

	// only update output every 'discretize' time
	if (++discretize_cntr >= vcap_cfg.discretize) {
		discretize_cntr = 0;

		// determine whether we should be in a new state
		if (is_outputting &&
		    (new_cap_voltage < vcap_cfg.lower_threshold_voltage)) {
			is_outputting = 0U; // we fall under our threshold
			virtcap_set_output_state(0U); // TODO: is_outputting and this fn each keep the same state ...
		} else if (!is_outputting &&(new_cap_voltage > vcap_cfg.upper_threshold_voltage)) {
			is_outputting = 1U; // we have enough voltage to switch on again
			virtcap_set_output_state(1U);
			new_cap_voltage = (new_cap_voltage >> 10) * outputcap_scale_factor; // TODO: magic numbers ... could be replaced by matching FN, analog to scale-calculation in init()
		}
	}

	cap_voltage = new_cap_voltage;
}

uint32_t SquareRootRounded(const uint32_t a_nInput)
{
	uint32_t op = a_nInput;
	uint32_t res = 0U;
	uint32_t one = 1uL << 30U;

	while (one > op) {
		one >>= 2u;
	}

	while (one != 0u) {
		if (op >= res + one) {
			op = op - (res + one);
			res = res + 2U * one;
		}
		res >>= 1U;
		one >>= 2U;
	}

	if (op > res) {
		res++;
	}

	return res;
}

static inline int32_t voltage_mv_to_logic(const int32_t voltage)
{
	/* Compensate for adc gain and offset, division for mv is split to optimize accuracy */
	int32_t logic_voltage = voltage * (cali_cfg.adc_load_voltage_gain / 100) / 10;
	logic_voltage += cali_cfg.adc_load_voltage_offset;
	return logic_voltage << SHIFT_VOLT;
}

static inline int32_t current_ua_to_logic(const int32_t current)
{
	/* Compensate for adc gain and offset, division for ua is split to optimize accuracy */
	int32_t logic_current = current * (cali_cfg.adc_load_current_gain / 1000) / 1000;
	/* Add 2^17 because current is defined around zero, not 2^17 */
	logic_current += cali_cfg.adc_load_current_offset + (1U << 17U); // TODO: why first remove 1<<17 and then add it again?
	return logic_current;
}

static void lookup_init()
{
	max_t1 = current_ua_to_logic(0.1 * 1e3); // TODO: these hardcoded values should be part of the config
	max_t2 = current_ua_to_logic(1 * 1e3);
	max_t3 = current_ua_to_logic(10 * 1e3);
	max_t4 = current_ua_to_logic(100 * 1e3);

	scale_index_t1 = 9 * (1U << SHIFT_LUT) / max_t1;
	scale_index_t2 = 9 * (1U << SHIFT_LUT) / max_t2;
	scale_index_t3 = 9 * (1U << SHIFT_LUT) / max_t3;
	scale_index_t4 = 9 * (1U << SHIFT_LUT) / max_t4;
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
static int32_t lookup(int32_t table[const][9], const int32_t current)
{
	if (current < max_t1) {
		const int32_t index = current * scale_index_t1 >> SHIFT_LUT;    // TODO: this looks wrong! int with bitshift, to index? without sign-test, without brackets to make precedence obvious
		return table[0][index];
	} else if (current < max_t2) {
        const int32_t index = current * scale_index_t2 >> SHIFT_LUT;
		return table[1][index];
	} else if (current < max_t3) {
        const int32_t index = current * scale_index_t3 >> SHIFT_LUT;
		return table[2][index];
	} else if (current < max_t4) {
        const int32_t index = current * scale_index_t4 >> SHIFT_LUT;
		return table[3][index];
	} else {
		// Should never get here
		return table[3][8];
	}
}

static void virtcap_set_output_state(const bool_ft value)
{
	VIRTCAP_OUT_PIN_state = value;

	if (value)  GPIO_ON(VIRTCAP_OUT_MASK);
	else 	    GPIO_OFF(VIRTCAP_OUT_MASK);
}

bool_ft virtcap_get_output_state()
{
	return VIRTCAP_OUT_PIN_state;
}
