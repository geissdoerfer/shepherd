import sys
import logging
import time
from pathlib import Path

logger = logging.getLogger(__name__)
sysfs_path = Path("/sys/shepherd")


class SysfsInterfaceException(Exception):
    pass

attribs = {
    "mode": {"path": "mode", "type": str},
    "state": {"path": "state", "type": str},
    "n_buffers": {"path": "n_buffers", "type": int},
    "buffer_period_ns": {"path": "buffer_period_ns", "type": int},
    "samples_per_buffer": {"path": "samples_per_buffer", "type": int},
    "harvesting_voltage": {"path": "harvesting_voltage", "type": int},
    "mem_address": {"path": "memory/address", "type": int},
    "mem_size": {"path": "memory/size", "type": int}
}

def wait_for_state(state: str, timeout: float):
    ts_start = time.time()
    while(True):
        current_state = get_state()
        if(current_state == state):
            return time.time() - ts_start

        if time.time() - ts_start > timeout:
            raise SysfsInterfaceException(
                (
                    f"timed out waiting for state { state } - "
                    f"state is { current_state }"
                )
            )
        time.sleep(0.1)


def start(start_time: int = None):
    current_state = get_state()
    if(current_state != "idle"):
        raise SysfsInterfaceException(
            f"Cannot start from state { current_state }")

    with open(str(sysfs_path / "state"), "w") as f:
        if start_time is None:
            f.write('start')
        else:
            f.write(f'{ start_time }')


def stop():
    current_state = get_state()
    if(current_state != "running"):
        raise SysfsInterfaceException(
            f"Cannot stop from state { current_state }")

    with open(str(sysfs_path / "state"), "w") as f:
        f.write('stop')


def set_mode(mode: str):
    if mode not in ['harvesting', 'load', 'emulation']:
        raise SysfsInterfaceException('invalid value for mode')
    if(get_state() != "idle"):
        raise SysfsInterfaceException(
            f"Cannot set mode when shepherd is { get_state() }"
        )
    with open(str(sysfs_path / 'mode'), "w") as f:
        f.write(mode)


def set_harvesting_voltage(harvesting_voltage: int):
    if(get_state() != "idle"):
        raise SysfsInterfaceException(
            f"Cannot set voltage when shepherd is { get_state() }"
        )
    mode = get_mode()
    if mode != "harvesting":
        raise SysfsInterfaceException(
            f"setting of harvesting voltage only possible in 'harvesting' mode"
        )
    with open(str(sysfs_path / 'harvesting_voltage'), "w") as f:
        f.write(f'{ harvesting_voltage }')


def make_attr_getter(name, path, attr_type):
    def _function():
        with open(str(sysfs_path / path), "r") as f:
            return attr_type(f.read().rstrip())
    return _function


for name, props in attribs.items():
    fun = make_attr_getter(name, props["path"], props["type"])
    setattr(sys.modules[__name__], f"get_{ name }", fun)
