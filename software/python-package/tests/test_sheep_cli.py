import pytest
import click
import numpy as np

from shepherd import LogWriter
from shepherd import CalibrationData
from shepherd.shepherd_io import DataBuffer
from shepherd.cli import cli


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


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_record(shepherd_up, cli_runner, tmp_path):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(cli, ["record", "-l", "10", "-o", f"{str(store)}"])

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_emulate(shepherd_up, cli_runner, tmp_path, data_h5):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli, ["emulate", "-l", "10", "-o", f"{str(store)}", f"{str(data_h5)}"]
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.parametrize("state", ["on", "off"])
def test_targetpower(state, shepherd_up, cli_runner):
    res = cli_runner.invoke(cli, ["targetpower", f"--{state}"])

    assert res.exit_code == 0
