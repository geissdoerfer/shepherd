#include "virtcap.h"

static uint32_t voltage_to_logic (float voltage);

// Constant variables
uint32_t kMaxCapVoltage;
uint32_t kMinCapVoltage;
uint32_t kInitCapVoltage;
uint32_t kDcoutputVoltage;
uint32_t kMaxVoltageOut;
uint32_t kMaxCurrentIn;
uint32_t kLeakageCurrent;
uint32_t kEfficiency;
uint32_t kOnTimeLeakageCurrent;

/* These are two variables used to keep calculations in 32bit range, note that  harv_div_scale * harv_mul_scale == 1000 */
const int32_t harv_div_scale = 10;
const int32_t harv_mul_scale = 100;

static uint32_t voltage_to_logic (float voltage)
{
  float value = voltage * 4095 * 1000 * 1000 / 3.3;
  return (uint32_t) roundf(value);
}

static uint32_t current_mA_to_logic (float current_mA)
{
  float value =  current_mA * 4095 * 1000 / 33;
  return (uint32_t) roundf(value);
}

void virtcap_init(float upper_threshold_voltage, float lower_threshold_voltage, uint32_t sample_period_us, uint32_t capacitance_uF)
{
  kMaxCapVoltage = voltage_to_logic(3.3);
  kMinCapVoltage = 0;
  kInitCapVoltage = voltage_to_logic(1.0);
  kDcoutputVoltage = voltage_to_logic(3.3);
  kMaxVoltageOut = 3300; // mV
  kMaxCurrentIn = 33; // mA
  kLeakageCurrent = current_mA_to_logic(0.022);
  kEfficiency = 47; // defined in %
  kOnTimeLeakageCurrent = current_mA_to_logic(0.012);
  
  // Calculate harvest multiplier
  harvest_multiplier = capacitance_uF * kMaxVoltageOut / kMaxCurrentIn / sample_period_us / harv_div_scale;

  // set threshold voltages
  upper_threshold_voltage = voltage_to_logic(upper_threshold_voltage);
  lower_threshold_voltage = voltage_to_logic(lower_threshold_voltage);

  // Initialize vars
  cap_voltage = kInitCapVoltage;
  is_outputting = 0;
  is_enabled = 0;

  // Initialize 'DC-DC' output to false
  _GPIO_OFF(PRU0_DEBUG);

}

void Update(uint32_t current_measured, uint32_t voltage_measured)
{
  
  // First check whether we are enabled
  // if (!is_enabled)
  // {
  //   return; // we are not enabled yet!
  // }

  current_measured *= 1000; // convert from mA to uA
  voltage_measured *= 1000; // convert from V to mV

  // calculate input current based on input power and input (cap) voltage
  int32_t input_power = ring_buffer.read();

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

  const float kCorrectionFactor = (1.0 / 1.15);
  const uint32_t kScaleInput = roundf( ((4095 * 1000) / 0.033) * ((4095 * 1000) / 3.3) / 1000000 * kCorrectionFactor / 1000 );

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

  // TODO remove debug
  #if 1
  static int16_t cntr;
  if (cntr++ % 1000 == 0)
  {
    UpdatePrintValues(input_power, input_current, output_current, current_measured, new_cap_voltage, voltage_measured);
  } 
  #endif

  #if 1
  // Make sure the voltage does not go beyond it's boundaries
  if (new_cap_voltage >= kMaxCapVoltage)
  {
    new_cap_voltage = kMaxCapVoltage;
  }
  else if (new_cap_voltage < 0)
  {
    new_cap_voltage = 0;
  }
  #endif

  sample_counter++;

  // determine whether we should be in a new state
  if (is_outputting && (new_cap_voltage < lower_threshold_voltage))
  {
    is_outputting = false; // we fall under our threshold
    set_dc_output(false);

    // handle duty cycle measurement
    last_on_duration = sample_counter;
    sample_counter = 0;
  }
  else if (!is_outputting && (new_cap_voltage > upper_threshold_voltage))
  {
    is_outputting = true; // we have enough voltage to switch on again
    set_dc_output(true);

    // handle duty cycle measurement
    last_off_duration = sample_counter;
    sample_counter = 0;
    duty_cycle_updated = true;
  }

  analogWrite(dac_pin, (int) new_cap_voltage / 1000000);
  cap_voltage = new_cap_voltage;
}

void Enable()
{
  is_enabled = true;
}

void Disable()
{
  is_enabled = false;
}

bool IsEnabled()
{
  return is_enabled;
}


bool GetOutputState()
{
  return is_outputting;
}

void PrintDutyCycle()
{
  if (duty_cycle_updated)
  {
    duty_cycle_updated = false;
    Serial.printf("%d,%d\n", last_on_duration, last_off_duration);
  }
}

void UpdatePrintValues(uint32_t input_power, int32_t input_current, uint32_t output_current, 
  uint32_t current_measured, uint32_t new_cap_voltage, uint32_t voltage_measured)
{
  input_power = input_power;
  input_current = input_current;
  output_current = output_current;
  current_measured = current_measured;
  new_cap_voltage = new_cap_voltage;
  voltage_measured = voltage_measured;

  print_values_updated = true;
}

void PrintValues()
{
  if (print_values_updated)
  {
    print_values_updated = false;
    Serial.printf("input_power: %lu, input_current: %ld, output_current: %lu,"
      "current_measured: %lu, voltage_measured: %lu, cap_voltage: %lu\n", 
      input_power, input_current, output_current, current_measured, voltage_measured, new_cap_voltage);
    Serial.printf("(input_current - output_current) * %d = %ld\n", harv_mul_scale, (input_current - output_current) );
    Serial.printf("(input_current - output_current) * %d / harvest_multiplier = %ld\n", harv_mul_scale, (int32_t) (input_current - output_current) * harv_mul_scale / harvest_multiplier);
  }
}