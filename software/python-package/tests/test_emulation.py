import pytest
from pathlib import Path
import numpy as np

from shepherd.shepherd_io import DataBuffer

from shepherd import LogWriter
from shepherd import LogReader
from shepherd import Emulator
from shepherd import CalibrationData
from shepherd import ShepherdIOException


def random_data(len):
    return np.random.randint(0, high=2 ** 18, size=len, dtype="u4")


@pytest.fixture()
def data_buffer():
    len_ = 10_000
    voltage = random_data(len_)
    current = random_data(len_)
    data = DataBuffer(voltage, current, 1551848387472)
    return data


@pytest.fixture
def data_h5(tmp_path):
    name = tmp_path / "record_example.h5"
    with LogWriter(name, CalibrationData.from_default()) as store:
        for i in range(100):
            len_ = 10_000
            fake_data = DataBuffer(random_data(len_), random_data(len_), i)
            store.write_data(fake_data)
    return name


@pytest.fixture()
def log_writer(tmp_path):
    calib = CalibrationData.from_default()
    with LogWriter(
        force=True,
        store_path=tmp_path / "test.h5",
        mode="load",
        calibration_data=calib,
    ) as lw:
        yield lw


@pytest.fixture()
def log_reader(data_h5):
    with LogReader(data_h5, 10000) as lr:
        yield lr


@pytest.fixture()
def emulator(request, shepherd_up, log_reader):
    emu = Emulator(
        calibration_recording=log_reader.get_calibration_data(),
        calibration_emulation=CalibrationData.from_default(),
    )
    request.addfinalizer(emu.__del__)
    emu.__enter__()
    request.addfinalizer(emu.__exit__)
    return emu


@pytest.mark.hardware
def test_emulation(log_writer, log_reader, emulator):
    # Preload emulator with some data
    for idx, buffer in enumerate(log_reader.read_blocks(end=64)):
        emulator.put_buffer(idx, buffer)

    emulator.start_sampling()
    emulator.wait_for_start(15)
    for hrvst_buf in log_reader.read_blocks(start=64):
        idx, load_buf = emulator.get_buffer(timeout=1)
        log_writer.write_data(load_buf)
        emulator.put_buffer(idx, hrvst_buf)

    for _ in range(63):
        idx, load_buf = emulator.get_buffer(timeout=1)
        log_writer.write_data(load_buf)

    with pytest.raises(ShepherdIOException):
        idx, load_buf = emulator.get_buffer(timeout=1)
