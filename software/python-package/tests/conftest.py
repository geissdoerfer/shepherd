import pytest
import linecache
import tokenize
import py
import subprocess
import time

import shepherd
from shepherd import sysfs_interface

def check_beagleboard():
    try:
        with open("/proc/cpuinfo") as info:
            if "AM33XX" in info.read():
                return True
    except Exception:
        pass
    return False


@pytest.fixture(
    params=[
        pytest.param("real_hardware", marks=pytest.mark.hardware),
        pytest.param("fake_hardware", marks=pytest.mark.fake_hardware),
    ]
)
def fake_hardware(request):
    if request.param == "fake_hardware":
        request.fixturenames.append("fs")
        fake_sysfs = request.getfixturevalue("fs")
        fake_sysfs.create_dir("/sys/class/remoteproc/remoteproc1")
        fake_sysfs.create_dir("/sys/class/remoteproc/remoteproc2")
        yield fake_sysfs
    else:
        yield None


def pytest_addoption(parser):
    parser.addoption(
        "--fake", action="store_true", default=False,
        help="run fake hardware tests"
    )


def pytest_collection_modifyitems(config, items):
    skip_fake = pytest.mark.skip(reason="need --fake option to run")
    skip_real = pytest.mark.skip(reason="selected fake hardware only")
    skip_missing_hardware = pytest.mark.skip(reason="no hardware to test on")
    real_hardware = check_beagleboard()

    for item in items:
        if "hardware" in item.keywords:
            if not real_hardware:
                item.add_marker(skip_missing_hardware)
            if config.getoption("--fake"):
                item.add_marker(skip_real)
        if "fake_hardware" in item.keywords and not config.getoption("--fake"):
            item.add_marker(skip_fake)

@pytest.fixture()
def kernel_module_up(fake_hardware):
    if fake_hardware is None:
        subprocess.run(["modprobe", "shepherd"])
        time.sleep(3)


@pytest.fixture()
def shepherd_up(fake_hardware, kernel_module_up):
    if fake_hardware is not None:
        files = [
            ("/sys/kernel/shepherd/state", "idle"),
            ("/sys/kernel/shepherd/mode", "harvesting"),
            ("/sys/kernel/shepherd/harvesting_voltage", "0"),
            ("/sys/kernel/shepherd/n_buffers", "1"),
            ("/sys/kernel/shepherd/memory/address", "1"),
            ("/sys/kernel/shepherd/memory/size", "1"),
            ("/sys/kernel/shepherd/samples_per_buffer", "1"),
            ("/sys/kernel/shepherd/buffer_period_ns", "1")
        ]
        for file_, content in files:
            fake_hardware.create_file(file_, contents=content)
        return

    with open("/sys/shepherd/state", "w") as f:
        f.write('stop')
    time.sleep(2)
    yield
    with open("/sys/shepherd/state", "w") as f:
        f.write('stop')


@pytest.fixture()
def shepherd_down(fake_hardware):
    if fake_hardware is None:
        subprocess.run(["rmmod", "shepherd"])