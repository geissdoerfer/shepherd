import pytest
from pathlib import Path

from shepherd import EEPROM
from shepherd import CapeData
from shepherd import CalibrationData


@pytest.fixture()
def data_calibration():
    return CalibrationData.from_default()


@pytest.fixture
def data_example_yml():
    here = Path(__file__).absolute()
    name = "capedata_example.yml"
    return here.parent / name


@pytest.fixture()
def data_example(data_calibration):
    data = CapeData.from_values("011900000001", "00A0")
    return data


@pytest.fixture()
def data_test_string():
    return "test content".encode("utf-8")


@pytest.fixture()
def eeprom_open(request, fake_hardware):
    if fake_hardware is not None:
        fake_hardware.create_file(
            "/sys/bus/i2c/devices/2-0054/eeprom", st_size=32768
        )
        request.applymarker(
            pytest.mark.xfail(
                raises=OSError, reason="pyfakefs doesn't support seek in files"
            )
        )
    with EEPROM() as eeprom:
        yield eeprom


@pytest.fixture()
def eeprom_retained(eeprom_open):
    data = eeprom_open._read(0, 1024)
    for i in range(256):
        eeprom_open._write(i * 4, b"\xDE\xAD\xBE\xEF")
    yield eeprom_open
    eeprom_open._write(0, data)


@pytest.fixture()
def eeprom_with_data(eeprom_retained, data_example):
    eeprom_retained.write_cape_data(data_example)
    return eeprom_retained


@pytest.fixture()
def eeprom_with_calibration(eeprom_retained, data_calibration):
    eeprom_retained.write_calibration(data_calibration)
    return eeprom_retained


def test_from_yaml(data_example_yml):
    data = CapeData.from_yaml(data_example_yml)
    assert data["serial_number"] == "0119XXXX0001"


@pytest.mark.eeprom
@pytest.mark.hardware
def test_read_raw(eeprom_open):
    eeprom_open._read(0, 4)


@pytest.mark.eeprom
@pytest.mark.hardware
def test_write_raw(eeprom_retained, data_test_string):
    eeprom_retained._write(0, data_test_string)
    data = eeprom_retained._read(0, len(data_test_string))
    assert data == data_test_string


@pytest.mark.eeprom
@pytest.mark.hardware
def test_read_value(eeprom_with_data, data_example):
    with pytest.raises(KeyError):
        datum = eeprom_with_data["some non-sense parameter"]
    assert eeprom_with_data["version"] == data_example["version"]


@pytest.mark.eeprom
@pytest.mark.hardware
def test_write_value(eeprom_retained, data_example):
    with pytest.raises(KeyError):
        eeprom_retained["some non-sense parameter"] = "some data"

    eeprom_retained["version"] = "1234"
    assert eeprom_retained["version"] == "1234"


@pytest.mark.eeprom
@pytest.mark.hardware
def test_write_capedata(eeprom_retained, data_example):
    eeprom_retained.write_cape_data(data_example)
    for key, value in data_example.items():
        if type(value) is str:
            assert eeprom_retained[key] == value.rstrip("\0")
        else:
            assert eeprom_retained[key] == value


@pytest.mark.eeprom
@pytest.mark.hardware
def test_read_capedata(eeprom_with_data, data_example):
    cape_data = eeprom_with_data.read_cape_data()
    for key in data_example.keys():
        assert data_example[key] == cape_data[key]


@pytest.mark.eeprom
@pytest.mark.hardware
def test_write_calibration(eeprom_retained, data_calibration):
    eeprom_retained.write_calibration(data_calibration)
    calib_restored = eeprom_retained.read_calibration()
    for component in ["harvesting", "load", "emulation"]:
        for channel in ["voltage", "current"]:
            for parameter in ["gain", "offset"]:
                assert (
                    calib_restored[component][channel][parameter]
                    == data_calibration[component][channel][parameter]
                )


@pytest.mark.eeprom
@pytest.mark.hardware
def test_read_calibration(eeprom_with_calibration, data_calibration):
    calib_restored = eeprom_with_calibration.read_calibration()
    for component in ["harvesting", "load", "emulation"]:
        for channel in ["voltage", "current"]:
            for parameter in ["gain", "offset"]:
                assert (
                    calib_restored[component][channel][parameter]
                    == data_calibration[component][channel][parameter]
                )
