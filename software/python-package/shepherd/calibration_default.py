# -*- coding: utf-8 -*-

"""
shepherd.calibration_default
~~~~~
Contains some info about the hardware configuration on the shepherd
cape. Based on these values, one can derive the expected adc readings given
an input voltage/current or, for emulation, the expected voltage and current
given the digital code in the DAC.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

# both current channels have 511mOhm shunt resistance
R_SHT = 2.0  # [ohm]
# the instrumentation amplifiers are configured for gain of 50.25
G_INST_AMP = 50.25  # [n]
# we use the ADC's internal reference with 4.096V
V_REF_ADC = 4.096  # [V]
# range of current channels is +/- 0.625 * V_REF
G_ADC_I = 0.625  # [gain / V_REF]
# range of voltage channels is 1.25 * V_REF
G_ADC_V = 1.25  # [gain / V_REF]
# bit resolution of ADC (of positive side)
M_ADCc = 17  # [bit]
M_ADCv = 18  # [bit]
# DACs has internal 2.5V reference
V_REF_DAC = 2.5  # [V]
# gain of DAC-A (current) is set to 1
G_DAC_I = 1  # [n]
# gain of DAC-B (voltage) is set to 2
G_DAC_V = 2  # [n]
# bias voltage is generated from V_REF_DAC based on 1.2k and 100k resistors
G_BIAS_AMP = 1.2 / 100
# bit resolution of DAC
M_DAC = 16  # [bit]

# resistor values used in current source
R_CS_REF_L = 1200  # [ohm]
R_CS_REF_H = 402  # [ohm]
R_CS_OUT = 30  # [ohm]


def current_to_adc(current: float) -> int:
    # voltage drop over shunt
    v_sht = R_SHT * current
    # voltage on input of adc
    v_adc = G_INST_AMP * v_sht
    # digital value according to ADC gain
    return int((2 ** M_ADCc) + v_adc * (2 ** M_ADCc) / (G_ADC_I * V_REF_ADC))


def adc_to_current(i_raw: int) -> float:
    # voltage on input of adc
    v_adc = float(i_raw - (2 ** M_ADCc)) * (G_ADC_I * V_REF_ADC) / (2 ** M_ADCc)
    # current according to adc value
    return v_adc / (R_SHT * G_INST_AMP)


def voltage_to_adc(voltage: float) -> int:
    # digital value according to ADC gain
    return int(voltage * (2 ** M_ADCv) / (G_ADC_V * V_REF_ADC))


def adc_to_voltage(v_raw: int) -> float:
    # voltage according to ADC value
    return float(v_raw) * (G_ADC_V * V_REF_ADC) / (2 ** M_ADCv)


def dac_to_voltage(value: int) -> float:
    return float(value) * (V_REF_DAC * G_DAC_V) / (2 ** M_DAC)


def voltage_to_dac(voltage: float) -> int:
    return int(voltage * (2 ** M_DAC) / (V_REF_DAC * G_DAC_V))


def dac_to_current(value: int) -> float:
    # voltage at DAC output
    v_dac = float(value) * (V_REF_DAC * G_DAC_I) / (2 ** M_DAC)
    # voltage after biasing with differential amplifier
    v_dac_biased = v_dac - G_BIAS_AMP * V_REF_DAC
    # current through ground referenced path of current source
    i_gnd_ref = v_dac_biased / R_CS_REF_L
    # voltage over reference resistor on high side of ground referenced path
    v_gnd_ref_h = R_CS_REF_H * i_gnd_ref
    # current through resistor on high side of output path
    return v_gnd_ref_h / R_CS_OUT
