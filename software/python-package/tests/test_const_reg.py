import pytest
from shepherd.const_reg import DAC6571


@pytest.fixture()
def dac():
    with DAC6571() as dac:
        yield dac


def test_instantiation():
    with DAC6571() as dac:
        pass


def test_write(dac):
    dac.write(0)
    dac.write(1023)
    with pytest.raises(ValueError):
        dac.write(1024)
    with pytest.raises(ValueError):
        dac.write(-1)


def test_set_voltage(dac):
    dac.set_voltage(0.0)
    dac.set_voltage(3.29)
    with pytest.raises(ValueError):
        dac.set_voltage(3.3)
    with pytest.raises(ValueError):
        dac.set_voltage(-1.0)
