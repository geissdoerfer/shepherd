import pytest
import numpy as np
import time
import logging
from pathlib import Path
import h5py
from collections import namedtuple
from itertools import product

from shepherd import LogReader
from shepherd import LogWriter
from shepherd import CalibrationData
from shepherd.datalog import DataBuffer
from shepherd.datalog import ExceptionRecord


def random_data(len):
    return np.random.randint(0, high=2 ** 18, size=len, dtype="u4")


@pytest.fixture()
def data_buffer():
    len_ = 10_000
    voltage = random_data(len_)
    current = random_data(len_)
    data = DataBuffer(1551848387472, len_, voltage, current)
    return data


@pytest.fixture
def data_nc():
    here = Path(__file__).absolute()
    name = here.parent / "record_example.h5"
    if not name.exists():
        logging.error(f"Missing {name}")
        logging.warning("We are recreating sample dataset")
        with LogWriter(
                name, CalibrationData.from_default()) as store:
            for i in range(100):
                len_ = 10_000
                fake_data = DataBuffer(
                    i,
                    len_,
                    random_data(len_),
                    random_data(len_)
                )
                store.write_data(fake_data)
        #raise IOError("Are you sure it is what you wanted?")
    return name


@pytest.fixture
def calibration_data():
    cd = CalibrationData.from_default()
    return cd


@pytest.mark.parametrize("mode", ["load", "harvesting"])
def test_create_logwriter(mode, tmp_path, calibration_data):
    d = tmp_path / "harvest.h5"
    h = LogWriter(
        store_name=str(d), calibration_data=calibration_data, mode=mode)
    assert not d.exists()
    h.__enter__()
    assert d.exists()
    h.__exit__()


def test_create_logwriter_with_force(tmp_path, calibration_data):
    d = tmp_path / "harvest.h5"
    d.touch()
    stat = d.stat()
    time.sleep(0.1)
    with pytest.raises(FileExistsError):
        h = LogWriter(
            store_name=str(d), calibration_data=calibration_data, force=False)
    h = LogWriter(
        store_name=str(d), calibration_data=calibration_data, force=True)
    h.__enter__()
    h.__exit__()
    new_stat = d.stat()
    assert new_stat.st_mtime > stat.st_mtime


@pytest.mark.parametrize("mode", ["load", "harvesting"])
def test_logwriter_data(
        mode, tmp_path, data_buffer, calibration_data):
    d = tmp_path / "harvest.h5"
    with LogWriter(
            store_name=d, calibration_data=calibration_data, mode=mode) as log:
        log.write_data(data_buffer)

    with h5py.File(d, "r") as written:

        assert "data" in written.keys()
        assert "time" in written["data"].keys()
        assert mode in written["data"].keys()
        for variable in ["voltage", "current"]:
            assert variable in written["data"][mode].keys()
            ref_var = getattr(data_buffer, variable)
            assert all(written["data"][mode][variable][:] == ref_var)


@pytest.mark.parametrize("mode", ["load", "harvesting"])
def test_calibration_logging(mode, tmp_path, calibration_data):
    d = tmp_path / "recording.h5"
    with LogWriter(
        store_name=d, mode=mode, calibration_data=calibration_data
    ) as _:
        pass

    h5store = h5py.File(d, 'r')

    for channel, parameter in product(["voltage", "current"], ["gain", "offset"]):
        assert (
            h5store["data"][mode][channel].attrs[parameter]
            == calibration_data["load"][channel][parameter]
        )


def test_exception_logging(
        tmp_path, data_buffer, calibration_data):
    d = tmp_path / "harvest.h5"

    with LogWriter(
            store_name=d, calibration_data=calibration_data) as writer:
        writer.write_data(data_buffer)

        ts = int(time.time() * 1000)
        writer.write_exception(
            ExceptionRecord(ts, "there was an exception", 0))
        writer.write_exception(
            ExceptionRecord(ts+1, "there was another exception", 1))
        assert writer.log_grp["message"][0] == "there was an exception"
        assert writer.log_grp["message"][1] == "there was another exception"
        assert writer.log_grp["value"][0] == 0
        assert writer.log_grp["value"][1] == 1
        assert writer.log_grp["time"][0] == ts
        assert writer.log_grp["time"][1] == ts+1


def test_key_value_store(
        tmp_path, calibration_data):
    d = tmp_path / "harvest.h5"

    with LogWriter(
            store_name=d, calibration_data=calibration_data) as writer:

        writer["some string"] = "this is a string"
        writer["some value"] = 5

    with h5py.File(d, 'r+') as hf:
        assert hf.attrs["some value"] == 5
        assert hf.attrs["some string"] == "this is a string"


@pytest.mark.timeout(1)
def test_logwriter_performance(
        tmp_path, data_buffer, calibration_data):
    d = tmp_path / "harvest.h5"
    with LogWriter(store_name=d, calibration_data=calibration_data) as log:
        log.write_data(data_buffer)


def test_logreader_performance(data_nc):
    read_durations = []
    with LogReader(store_name=data_nc, block_len=10000) as reader:
        past = time.time()
        for data in reader.read_blocks():
            now = time.time()
            elapsed = now - past
            read_durations.append(elapsed)
            past = time.time()
    assert np.mean(read_durations) < 0.05
