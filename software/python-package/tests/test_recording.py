import pytest
import logging
import h5py
import time

from shepherd import LogWriter
from shepherd import Recorder
from shepherd import record
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
def test_recorder(log_writer, recorder):
    recorder.start(wait_blocking=False)
    recorder.wait_for_start(15)

    for _ in range(100):
        idx, buf = recorder.get_buffer()
        log_writer.write_buffer(buf)
        recorder.release_buffer(idx)


@pytest.mark.hardware
@pytest.mark.timeout(30)
def test_record_fn(tmp_path, shepherd_up):
    d = tmp_path / "rec.h5"
    start_time = int(time.time() + 10)
    record(
        store_path=d,
        mode="harvesting",
        length=10,
        force=True,
        defaultcalib=True,
        harvesting_voltage=None,
        load="artificial",
        init_charge=False,
        start_time=start_time,
    )

    with h5py.File(d, "r+") as hf:
        n_samples = hf["data"]["time"].shape[0]
        assert 900_000 < n_samples <= 1_100_000
        assert hf["data"]["time"][0] == start_time * 1e9
