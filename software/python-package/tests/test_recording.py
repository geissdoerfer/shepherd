import pytest
import logging

from shepherd import LogWriter
from shepherd import Recorder
from shepherd import CalibrationData

consoleHandler = logging.StreamHandler()
logger = logging.getLogger("shepherd")
logger.addHandler(consoleHandler)
logger.setLevel(logging.DEBUG)


@pytest.fixture(params=["harvesting", "load"])
def mode(request):
    return request.param


@pytest.fixture()
def log_writer(tmp_path, mode):
    calib = CalibrationData.from_default()
    with LogWriter(
        mode=mode,
        calibration_data=calib,
        force=True,
        store_path=tmp_path / "test.h5",
    ) as lw:
        yield lw


@pytest.fixture()
def recorder(request, shepherd_up, mode):
    rec = Recorder(mode=mode)
    request.addfinalizer(rec.__del__)
    rec.__enter__()
    request.addfinalizer(rec.__exit__)
    return rec


@pytest.mark.hardware
def test_instantiation(shepherd_up):
    rec = Recorder()
    rec.__enter__()
    assert rec is not None
    rec.__exit__()
    del rec


@pytest.mark.hardware
def test_recording(log_writer, recorder):
    recorder.start_sampling()
    recorder.wait_for_start(15)

    for _ in range(100):
        idx, buf = recorder.get_buffer()
        log_writer.write_data(buf)
        recorder.release_buffer(idx)
