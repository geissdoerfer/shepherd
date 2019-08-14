#pragma once

#include <stdint.h>

// dac functions
void set_dc_output(bool output);

const uint32_t kMaxCapVoltage = voltage_to_logic(3.3);
const uint32_t kMinCapVoltage = 0;
const uint32_t kInitCapVoltage = voltage_to_logic(1.0);
const uint32_t kDcoutputVoltage = voltage_to_logic(3.3);
const uint32_t kMaxVoltageOut = 3300; // mW
const uint32_t kMaxCurrentIn = 33; // mA
const uint32_t kLeakageCurrent = current_mA_to_logic(0.022);
const uint32_t kEfficiency = 47; // defined in %
const uint32_t kOnTimeLeakageCurrent = current_mA_to_logic(0.012);

void Init(float upper_threshold_voltage, float lower_threshold_voltage, uint32_t sample_period_us, uint32_t capacitance_uF);
void Enable();
void Disable();
bool IsEnabled();
void UpdatePrintValues(uint32_t input_power, int32_t input_current, uint32_t output_current, 
  uint32_t current_measured, uint32_t new_cap_voltage, uint32_t voltage_measured);
void PrintValues();
void PrintDutyCycle();

void Update(uint32_t current_measured, uint32_t voltage_measured);
void WriteBuffer(int value);
bool IsBufferFull();
bool GetOutputState();

// debug vars
uint32_t input_power;
int32_t input_current;
int32_t output_current;
uint32_t current_measured;
uint32_t new_cap_voltage;
uint32_t voltage_measured;

bool print_values_updated = false;

// other vars
uint32_t upper_threshold_voltage;
uint32_t lower_threshold_voltage;
uint16_t sample_period_us;
int32_t harvest_multiplier;

RingBuffer ring_buffer;
uint32_t cap_voltage = kInitCapVoltage;
bool is_outputting = false;
bool is_enabled = false;

// sample counter
int sample_counter = 0;
int last_on_duration = 0;
int last_off_duration = 0;
bool duty_cycle_updated = false;