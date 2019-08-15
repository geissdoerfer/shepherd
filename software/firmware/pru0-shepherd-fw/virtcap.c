#include <stdint.h>

#include "gpio.h"
#include "hw_config.h"

#include "virtcap.h"

static uint32_t voltage_to_logic (uint32_t voltage);
static uint32_t current_mA_to_logic (uint32_t current_mA);

// Constant variables
static uint32_t kMaxCapVoltage;
static uint32_t kMinCapVoltage;
static uint32_t kInitCapVoltage;
static uint32_t kDcoutputVoltage;
static uint32_t kMaxVoltageOut;
static uint32_t kMaxCurrentIn;
static uint32_t kLeakageCurrent;
static uint32_t kEfficiency;
static uint32_t kOnTimeLeakageCurrent;

/* Simulating inputs, should later be replaced by measurments from the system*/ 
static uint32_t current_measured_sim;
static uint32_t voltage_measured_sim;
static uint32_t input_power_sim;

/* These are two variables used to keep calculations in 32bit range, note that  harv_div_scale * harv_mul_scale == 1000 */
static const int32_t harv_div_scale = 10;
static const int32_t harv_mul_scale = 100;

static const uint32_t sample_period_us = 10;

static uint32_t upper_threshold_voltage;
static uint32_t lower_threshold_voltage;

static int32_t harvest_multiplier;

// Initialize vars
static uint32_t cap_voltage;
static uint8_t is_outputting;
static uint8_t is_enabled;

static uint32_t kCorrectionFactor;
static uint32_t kScaleInput;

void virtcap_init(uint32_t upper_threshold_voltage_param, uint32_t lower_threshold_voltage_param, uint32_t capacitance_uF)
{
  /**
   * Input current calculation (1):
   * 
   * I_in = P_in / V_in
   * 
   * To compensate for the current (mA) being stored in logic values (2):
   * I_in = I'_in * 0.033 / (4095 * 1000)
   * 
   * To compensate for the voltage (V) being stored in logic values (3):
   * V_in = V'_in / 3.3 / (4095 * 1000) 
   * 
   * Convert power from uW to W (4): 
   * P_in = P'_in / 1000000
   * 
   * Give the input power a correction factor to increase accuracy (5):
   * P_in = P'_in / 1000000 * kCorrectionFactor
   * 
   * Since we are interested in the current in logic value, we combine (1) and (2), (6):
   * I_in = I'_in * 33 / (4095 * 1000)
   * I'_in = I_in * (4095 * 1000) / 33
   * I'_in = (P_in / V_in) * (4095 * 1000) / 33
   * 
   * Combining (6) and (3), (7):
   * I'_in = (P_in / ( V'_in / 3.3 / (4095 * 1000) ) * (4095 * 1000) / 33
   * I'_in = (P_in / V'_in)  * ((4095 * 1000) / 33) * ((4095 * 1000) / 3.3)
   * 
   * Combining (7) and (5), the final equation becomes (8):
   * I'_in = ((P'_in / 1000000 * kCorrectionFactor) / V'_in)  * ((4095 * 1000) / 33) * ((4095 * 1000) / 3.3)
   * I'_in = (P'_in / V'_in) * ((4095 * 1000) / 0.033) * ((4095 * 1000) / 3.3) / 1000000 * kCorrectionFactor
   * 
   * We extract the constant part of this equation and we divide this by 1000 to reduce the resolution from ~28bits to ~18bits
   * 
   * Later in the final input current calculation the result will be multiplied with 1000 again.
   * This enables to stay within 32bits during calculation.
   */

  kCorrectionFactor = (1.0 / 1.15);
  kScaleInput = ((4095 * 1000 * 1000) / 33) * ((4095 * 1000 * 1000) / 3300) / 1000000 * kCorrectionFactor / 1000 ;

  // initializing constants
  kMaxCapVoltage = voltage_to_logic(3.3);
  kMinCapVoltage = 0;
  kInitCapVoltage = voltage_to_logic(1.0);
  kDcoutputVoltage = voltage_to_logic(3.3);
  kMaxVoltageOut = 3300; // mV
  kMaxCurrentIn = 33; // mA
  kLeakageCurrent = current_mA_to_logic(0.022);
  kEfficiency = 47; // defined in %
  kOnTimeLeakageCurrent = current_mA_to_logic(0.012);

  // initializing simulated constants
  current_measured_sim = current_mA_to_logic(6.6) / 1000;
  voltage_measured_sim = voltage_to_logic(2.5) / 1000000;
  input_power_sim = 4000; // defined in uW
  
  // Calculate harvest multiplier
  harvest_multiplier = capacitance_uF * kMaxVoltageOut / kMaxCurrentIn / sample_period_us / harv_div_scale;

  // set threshold voltages
  upper_threshold_voltage = voltage_to_logic(upper_threshold_voltage_param);
  lower_threshold_voltage = voltage_to_logic(lower_threshold_voltage_param);

  // Initialize vars
  cap_voltage = kInitCapVoltage;
  is_outputting = FALSE;
  is_enabled = FALSE;

  // Initialize 'DC-DC' output to false
  _GPIO_OFF(VIRTCAP_SLCT_LOAD);

}

void virtcap_update(uint32_t current_measured, uint32_t voltage_measured)
{
  current_measured = current_measured_sim;
  voltage_measured = voltage_measured_sim;

  current_measured *= 1000; // convert from mA to uA
  voltage_measured *= 1000; // convert from V to mV

  // calculate input current based on input power and input (cap) voltage
  int32_t input_power = input_power_sim; // TODO this should be formed by a varying input


  int32_t input_current = input_power * kScaleInput / (cap_voltage / 1000) * 1000;

  input_current -= kLeakageCurrent; // compensate for leakage current
  
  int32_t output_current;

  // compensate output current for higher voltage than input + boost converter efficiency
  if (is_outputting)
  {
    output_current = (voltage_measured) * 100 / (cap_voltage / 1000) * current_measured / kEfficiency; // + kOnTimeLeakageCurrent;
  } 
  else
  {
    output_current = 0; // force current read to zero while not outputting (to ignore noisy current reading)
  }

  uint32_t new_cap_voltage = cap_voltage + ((input_current - output_current) * harv_mul_scale / harvest_multiplier);

  #if 1
  // Make sure the voltage does not go beyond it's boundaries
  if (new_cap_voltage >= kMaxCapVoltage)
  {
    new_cap_voltage = kMaxCapVoltage;
  }
  else if (new_cap_voltage < kMinCapVoltage)
  {
    new_cap_voltage = kMinCapVoltage;
  }
  #endif


  // determine whether we should be in a new state
  if (is_outputting && (new_cap_voltage < lower_threshold_voltage))
  {
    is_outputting = FALSE; // we fall under our threshold
    _GPIO_OFF(VIRTCAP_SLCT_LOAD);
  }
  else if (!is_outputting && (new_cap_voltage > upper_threshold_voltage))
  {
    is_outputting = TRUE; // we have enough voltage to switch on again
    _GPIO_ON(VIRTCAP_SLCT_LOAD);
  }

  cap_voltage = new_cap_voltage;
}

static uint32_t voltage_to_logic (uint32_t voltage)
{
  uint32_t value = voltage * 4095 * 1000 * 1000 / 3.3;
  return (uint32_t) (value);
}

static uint32_t current_mA_to_logic (uint32_t current_mA)
{
  uint32_t value =  current_mA * 4095 * 1000 / 33;
  return (uint32_t) (value);
}
