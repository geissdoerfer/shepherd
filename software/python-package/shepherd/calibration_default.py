"""
This file contains some info about the hardware configuration on the shepherd
cape. Based on these values, one can derive the expected adc readings given
an input voltage/current or, for emulation, the expected voltage and current
given the digital code in the DAC.
"""

# both current channels have 511mOhm shunt resistance
R_SHT = 0.511
# the instrumentation amplifiers are configured for gain of 100
G_INST_AMP = 100
# we use the ADC's internal reference with 4.096V
V_REF_ADC = 4.096
# range of current channels is +/- 0.625 * V_REF
G_ADC_I = 0.625
# range of voltage channels is 1.25 * V_REF
G_ADC_V = 1.25
# DACs have external 3.0V reference
V_REF_DAC = 3.0
# bias voltage is generated from V_REF_DAC based on 1.2k and 100k resistors
G_BIAS_AMP = 1.2 / 100

# resistor values used in current source
R_CS_REF_L = 1200
R_CS_REF_H = 510
R_CS_OUT = 24


def current_to_adc(current: float):
    # voltage drop over shunt
    v_sht = R_SHT * current
    # voltage on input of adc
    v_adc = G_INST_AMP * v_sht
    # digital value according to ADC gain
    return (2 ** 17) + v_adc / (G_ADC_I * V_REF_ADC) * (2 ** 17 - 1)


def voltage_to_adc(voltage: float):
    # digital value according to ADC gain
    return voltage / (G_ADC_V * V_REF_ADC) * (2 ** 18 - 1)


def dac_to_voltage(value: int):
    return value / (2 ** 18 - 1) * V_REF_DAC


def dac_to_current(value: int):
    # voltage at DAC output
    v_dac = float(value) / (2 ** 18 - 1) * V_REF_DAC
    # voltage after biasing with differential amplifier
    v_dac_biased = v_dac - G_BIAS_AMP * V_REF_DAC
    # current through ground referenced path of current source
    i_gnd_ref = v_dac_biased / R_CS_REF_L
    # voltage over reference resistor on high side of ground referenced path
    v_gnd_ref_h = R_CS_REF_H * i_gnd_ref
    # current through resistor on high side of output path
    return v_gnd_ref_h / R_CS_OUT
