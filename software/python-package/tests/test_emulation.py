import pytest
from pathlib import Path
import numpy as np
import h5py
import time

from shepherd.shepherd_io import DataBuffer

from shepherd import LogWriter
from shepherd import LogReader
from shepherd import Emulator
from shepherd import emulate
from shepherd import CalibrationData
from shepherd import ShepherdIOException


def random_data(len):
    return np.random.randint(0, high=2 ** 18, size=len, dtype="u4")


@pytest.fixture
def data_h5(tmp_path):
    store_path = tmp_path / "record_example.h5"
    with LogWriter(store_path, CalibrationData.from_default()) as store:
        for i in range(100):
            len_ = 10_000
            fake_data = DataBuffer(random_data(len_), random_data(len_), i)
            store.write_buffer(fake_data)
    return store_path


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
        initial_buffers=log_reader.read_buffers(end=64),
    )
    request.addfinalizer(emu.__del__)
    emu.__enter__()
    request.addfinalizer(emu.__exit__)
    return emu


@pytest.mark.hardware
def test_emulation(log_writer, log_reader, emulator):

    emulator.start(wait_blocking=False)
    emulator.wait_for_start(15)
    for hrvst_buf in log_reader.read_buffers(start=64):
        idx, load_buf = emulator.get_buffer(timeout=1)
        log_writer.write_buffer(load_buf)
        emulator.put_buffer(idx, hrvst_buf)

    for _ in range(63):
        idx, load_buf = emulator.get_buffer(timeout=1)
        log_writer.write_buffer(load_buf)

    with pytest.raises(ShepherdIOException):
        idx, load_buf = emulator.get_buffer(timeout=1)


@pytest.mark.hardware
def test_emulate_fn(tmp_path, data_h5, shepherd_up):
    d = tmp_path / "rec.h5"
    start_time = int(time.time() + 15)
    emulate(
        harvestingstore_path=data_h5,
        loadstore_path=d,
        length=None,
        force=True,
        defaultcalib=True,
        load="artificial",
        init_charge=False,
        start_time=start_time,
    )

    with h5py.File(d, "r+") as hf_load, h5py.File(data_h5) as hf_hrvst:
        assert (
            hf_load["data"]["time"].shape[0]
            == hf_hrvst["data"]["time"].shape[0]
        )
        assert hf_load["data"]["time"][0] == start_time * 1e9
