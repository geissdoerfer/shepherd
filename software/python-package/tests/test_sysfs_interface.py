import pytest
import subprocess
import time
from pathlib import Path
import yaml

from shepherd import sysfs_interface
from shepherd.calibration import CalibrationData


@pytest.fixture()
def shepherd_running(shepherd_up):
    sysfs_interface.set_start()
    sysfs_interface.wait_for_state("running", 5)


@pytest.fixture()
def calibration_settings():
    calibration_emulation = CalibrationData.from_default()
    current_gain = int(1 / calibration_emulation["load"]["current"]["gain"])
    current_offset = int(
        calibration_emulation["load"]["current"]["offset"]
        / calibration_emulation["load"]["current"]["gain"]
    )
    voltage_gain = int(1 / calibration_emulation["load"]["voltage"]["gain"])
    voltage_offset = int(
        calibration_emulation["load"]["voltage"]["offset"]
        / calibration_emulation["load"]["voltage"]["gain"]
    )
    return current_gain, current_offset, voltage_gain, voltage_offset


@pytest.mark.hardware
@pytest.mark.parametrize("attr", sysfs_interface.attribs.keys())
def test_getters(shepherd_up, attr):
    method_to_call = getattr(sysfs_interface, f"get_{ attr }")
    assert method_to_call() != None


@pytest.mark.hardware
@pytest.mark.parametrize("attr", sysfs_interface.attribs.keys())
def test_getters_fail(shepherd_down, attr):
    method_to_call = getattr(sysfs_interface, f"get_{ attr }")
    with pytest.raises(FileNotFoundError):
        method_to_call()


@pytest.mark.hardware
def test_start(shepherd_up):
    sysfs_interface.set_start()
    time.sleep(5)
    assert sysfs_interface.get_state() == "running"
    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.set_start()


@pytest.mark.hardware
def test_wait_for_state(shepherd_up):
    sysfs_interface.set_start()
    assert sysfs_interface.wait_for_state("running", 3) < 3
    sysfs_interface.set_stop()
    assert sysfs_interface.wait_for_state("idle", 3) < 3


@pytest.mark.hardware
def test_start_delayed(shepherd_up):
    start_time = time.time() + 5
    sysfs_interface.set_start(start_time)

    sysfs_interface.wait_for_state("armed", 1)
    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.wait_for_state("running", 3)

    sysfs_interface.wait_for_state("running", 3)

    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.set_start()


@pytest.mark.parametrize("mode", ["harvesting", "load", "emulation"])
def test_set_mode(shepherd_up, mode):
    sysfs_interface.set_mode(mode)
    assert sysfs_interface.get_mode() == mode


def test_initial_mode(shepherd_up):
    assert sysfs_interface.get_mode() == "harvesting"


@pytest.mark.hardware
def test_set_mode_fail_offline(shepherd_running):
    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.set_mode("harvesting")


@pytest.mark.hardware
def test_set_mode_fail_invalid(shepherd_up):
    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.set_mode("invalidmode")


@pytest.mark.parametrize("value", [0, 100, 16000])
def test_harvesting_voltage(shepherd_up, value):
    sysfs_interface.set_harvesting_voltage(value)
    assert sysfs_interface.get_harvesting_voltage() == value


def test_initial_harvesting_voltage(shepherd_up):
    assert sysfs_interface.get_harvesting_voltage() == 0


@pytest.mark.hardware
@pytest.mark.parametrize("mode", ["emulation", "load"])
def test_harvesting_voltage_fail_mode(shepherd_up, mode):
    sysfs_interface.set_mode(mode)
    with pytest.raises(sysfs_interface.SysfsInterfaceException):
        sysfs_interface.set_harvesting_voltage(2 ** 15)
