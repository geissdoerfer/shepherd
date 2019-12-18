# -*- coding: utf-8 -*-

"""
test_sheep_cli
~~~~~
Tests the shepherd sheep CLI implemented with python click.

CAVEAT: For some reason, tests fail when invoking CLI two times within the
same test. Either find a solution or put every CLI call in a separate test.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

import pytest
import click
import numpy as np
from pathlib import Path

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
    res = cli_runner.invoke(
        cli, ["-vvv", "record", "-l", "10", "-o", f"{str(store)}"]
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_record_ldo_short(shepherd_up, cli_runner, tmp_path):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli, ["-vvv", "record", "-l", "10", "-o", f"{str(store)}", "-c", "2.5"]
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_record_ldo_explicit(shepherd_up, cli_runner, tmp_path):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "record",
            "-l",
            "10",
            "-o",
            f"{str(store)}",
            "--ldo-voltage",
            "2.5",
        ],
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_record_ldo_fail(shepherd_up, cli_runner, tmp_path):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "record",
            "-l",
            "10",
            "-f",
            "-o",
            f"{str(store)}",
            "-c",
            "-1.0",
        ],
    )
    assert res.exit_code != -1


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_record_no_calib(shepherd_up, cli_runner, tmp_path):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli, ["-vvv", "record", "-l", "10", "--no-calib", "-o", f"{str(store)}"]
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_emulate(shepherd_up, cli_runner, tmp_path, data_h5):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "emulate",
            "-l",
            "10",
            "-o",
            f"{str(store)}",
            f"{str(data_h5)}",
        ],
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_virtcap_emulate(shepherd_up, cli_runner, tmp_path, data_h5):
    here = Path(__file__).absolute()
    name = "virtcap_settings.yml"
    file_path = here.parent / name
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "emulate",
            "-l",
            "10",
            "--config",
            f"{str(file_path)}",
            "-o",
            f"{str(store)}",
            f"{str(data_h5)}",
        ],
    )

    assert res.exit_code == 0
    assert store.exists()


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_virtcap_emulate_wrong_option(
    shepherd_up, cli_runner, tmp_path, data_h5
):
    here = Path(__file__).absolute()
    name = "virtcap_settings.yml"
    file_path = here.parent / name
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "emulate",
            "-l",
            "10",
            "--virtcap",
            f"{str(file_path)}",
            "-o",
            f"{str(store)}",
            f"{str(data_h5)}",
        ],
    )

    assert res.exit_code != 0


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_emulate_ldo_short(shepherd_up, cli_runner, tmp_path, data_h5):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv",
            "emulate",
            "-l",
            "10",
            "-c",
            "2.5",
            "-o",
            f"{str(store)}",
            f"{str(data_h5)}",
        ],
    )

    assert res.exit_code == 0


@pytest.mark.hardware
@pytest.mark.timeout(60)
def test_emulate_ldo_fail(shepherd_up, cli_runner, tmp_path, data_h5):
    store = tmp_path / "out.h5"
    res = cli_runner.invoke(
        cli,
        [
            "-vvv" "emulate",
            "-l",
            "10",
            "-c",
            "5.0",
            "-o",
            f"{str(store)}",
            f"{str(data_h5)}",
        ],
    )

    assert res.exit_code != 0


@pytest.mark.hardware
@pytest.mark.parametrize("state", ["on", "off"])
def test_targetpower(state, shepherd_up, cli_runner):
    res = cli_runner.invoke(cli, ["targetpower", f"--{state}"])
    assert res.exit_code == 0


@pytest.mark.hardware
def test_targetpower_voltage(shepherd_up, cli_runner):
    res = cli_runner.invoke(cli, ["-vvv", "targetpower", "--voltage", "2.1"])
    assert res.exit_code == 0

    res = cli_runner.invoke(
        cli, ["-vvv", "targetpower", "--on", "--voltage", "3.3"]
    )
    assert res.exit_code == 0

    res = cli_runner.invoke(
        cli, ["-vvv", "targetpower", f"--on", "--voltage", "10"]
    )
    assert res.exit_code != 0

    res = cli_runner.invoke(
        cli, ["-vvv", "targetpower", f"--off", "--voltage", "3.0"]
    )
    assert res.exit_code != 0

