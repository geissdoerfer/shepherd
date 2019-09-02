import pytest
from shepherd.const_reg import DAC6571
from shepherd.const_reg import VariableLDO


@pytest.fixture()
def dac():
    with DAC6571() as dac:
        yield dac


@pytest.fixture()
def ldo():
    with VariableLDO() as ldo:
        yield ldo


def test_dac_instantiation():
    with DAC6571() as dac:
        pass


def test_ldo_instantiation():
    with VariableLDO() as ldo:
        pass


def test_dac_write(dac):
    dac.write(0)
    dac.write(1023)
    with pytest.raises(ValueError):
        dac.write(1024)
    with pytest.raises(ValueError):
        dac.write(-1)


def test_dac_set_voltage(dac):
    dac.set_voltage(0.0)
    dac.set_voltage(3.29)
    with pytest.raises(ValueError):
        dac.set_voltage(5.0)
    with pytest.raises(ValueError):
        dac.set_voltage(-1.0)


@pytest.mark.parametrize("state", [True, False])
def test_ldo_enable(ldo, state):
    ldo.set_output(state)


def test_ldo_set_voltage(ldo):
    ldo.set_voltage(1.6)
    ldo.set_voltage(3.6)
    with pytest.raises(ValueError):
        ldo.set_voltage(1.0)
    with pytest.raises(ValueError):
        ldo.set_voltage(5.0)
