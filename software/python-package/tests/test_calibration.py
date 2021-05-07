import pytest
from pathlib import Path

from shepherd import CalibrationData


@pytest.fixture
def data_meas_example_yml():
    here = Path(__file__).absolute()
    name = "example_calib_meas.yml"
    return here.parent / name


@pytest.fixture
def data_example_yml():
    here = Path(__file__).absolute()
    name = "example_calib.yml"
    return here.parent / name


@pytest.fixture()
def default_calib():
    calib = CalibrationData.from_default()
    return calib


@pytest.fixture()
def default_bytestr(default_calib):
    return default_calib.to_bytestr()


def test_from_default():
    calib = CalibrationData.from_default()


def test_from_yaml(data_example_yml):
    calib = CalibrationData.from_yaml(data_example_yml)


def test_from_measurements(data_meas_example_yml):
    calib = CalibrationData.from_measurements(data_meas_example_yml)


def test_to_bytestr(default_calib):
    default_calib.to_bytestr()


def test_from_bytestr(default_bytestr):
    calib = CalibrationData.from_bytestr(default_bytestr)
