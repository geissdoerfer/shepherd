import pytest
from pathlib import Path

from shepherd import LogWriter
from shepherd import LogReader
from shepherd import Emulator
from shepherd import CalibrationData
from shepherd import ShepherdIOException

@pytest.fixture
def data_nc():
    here = Path(__file__).absolute()
    name = "record_example.h5"
    return here.parent / name


@pytest.fixture()
def log_writer(tmp_path):
    calib = CalibrationData.from_default()
    with LogWriter(
            force=True, store_name=tmp_path / 'test.h5', mode='load',
            calibration_data=calib) as lw:
        yield lw


@pytest.fixture()
def log_reader(data_nc):
    with LogReader(data_nc, 10000) as lr:
        yield lr


@pytest.fixture()
def emulator(request, shepherd_up, log_reader):
    emu = Emulator(
            calibration_recording=log_reader.get_calibration_data(),
            calibration_emulation=CalibrationData.from_default()
    )
    request.addfinalizer(emu.__del__)
    emu.__enter__()
    request.addfinalizer(emu.__exit__)
    return emu


pytest.mark.hardware
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
