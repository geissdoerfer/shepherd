# -*- coding: utf-8 -*-

"""
shepherd.const_reg
~~~~~
The shepherd cape hosts a linear voltage regulator that can be used to supply
an attached sensor node with a software-configurable voltage. This can be used,
for example, for supplying the node with a constant voltage during programming.
This file provides a driver for the I2C DAC that is used to set the output
voltage of the regulator.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

from periphery import I2C
from periphery import GPIO

# resistor values for 10-bit DAC and LDO for fixed voltage power supply
R24 = 56_000
R25 = 40_200
R26 = 30_000

# feedback voltage of tps73101 LDO
V_FB_LDO = 1.2

PIN_LDO_ENABLE = 51


class DAC6571(object):
    """Driver for DAC6571 via Linux I2C interface

    The DAC6571 is connected to the BeagleBone's I2C bus. This class provides
    a driver layer accessing the Linux interface for the I2C bus, allowing to
    set the output voltage of the DAC by sending the corresponding commands.


    """

    def __init__(
        self, bus_number: int = 1, address: int = 0x4C, v_supply: float = 3.38
    ):
        """Initializes DAC by bus number and address.

        Args:
            bus_num (int): I2C bus number, e.g. 1 for I2C1 on BeagleBone
            address (int): I2C address of chip, hardware configurable
            v_supply (float): DAC supply voltage in volt
        """
        self._bus_number = bus_number
        self._address = address
        self._v_supply = v_supply

    def __enter__(self, *args):
        self._controller = I2C(f"/dev/i2c-{self._bus_number}")
        return self

    def __exit__(self, *args):
        self._controller.close()

    def write(self, value: int):
        """Writes digital value to DAC input register via I2C bus

        Args:
            value (int): Digital DAC code to be written to DAC input register
        """
        if not 0 <= value < 1024:
            raise ValueError(f"Value {value} outside of range for 10-bit DAC")

        msg_bytes = (value << 2).to_bytes(2, byteorder="big")
        msgs = [I2C.Message(msg_bytes)]
        self._controller.transfer(self._address, msgs)

    def set_voltage(self, voltage: float):
        """Sets voltage on DAC output

        Args:
            voltage (float): Desired voltage on DAC output
        """
        dac_code = int(voltage / self._v_supply * 1024)
        self.write(dac_code)


class VariableLDO(object):
    """Driver for DAC controlled LDO regulator

    The shepherd cape hosts a linear voltage regulator, that can be used to
    1) power a sensor node with a constant, but configurable voltage
    2) pre-charge the capacitor before starting recording/emulation
    The circuit consists of a TI TPS73101 LDO and a TI DAC6571 DAC. The DAC
    is connected to the feedback pin of the LDO and biases the current from
    the output pin, effectively controlling the output voltage of the LDO.
    """

    def __init__(self):
        self.dac = DAC6571()

    def __enter__(self, *args):
        self.enable_pin = GPIO(PIN_LDO_ENABLE, "out")
        self.enable_pin.write(False)
        self.dac.__enter__()
        return self

    def __exit__(self, *args):
        self.dac.__exit__()
        self.enable_pin.close()

    def set_output(self, state: bool):
        """Enables or disables LDO

        Args:
            state (bool): True to enable LDO, False to disable
        """
        if not state:
            self.dac.set_voltage(0.0)
        self.enable_pin.write(state)

    def set_voltage(self, voltage: float):
        """Sets the voltage on the output of the LDO

        Args:
            voltage (float): Voltage in volt. Must be between 1.6V and 3.6V due
                to hardware limitations.
        """
        if voltage > 3.6 or voltage < 1.6:
            raise ValueError("Fixed voltage must be between 1.6V and 3.6V")
        v_dac = V_FB_LDO - R24 * ((voltage - V_FB_LDO) / R25 - V_FB_LDO / R26)
        self.dac.set_voltage(v_dac)
