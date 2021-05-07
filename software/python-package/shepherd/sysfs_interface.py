# -*- coding: utf-8 -*-

"""
shepherd.sysfs_interface
~~~~~
Provides convenience functions for interacting with the sysfs interface
provided by the shepherd kernel module


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

import sys
import logging
import time
import struct
from pathlib import Path
from typing import NoReturn

logger = logging.getLogger(__name__)
sysfs_path = Path("/sys/shepherd")


class SysfsInterfaceException(Exception):
    pass


shepherd_modes = ["harvesting", "load", "emulation", "debug"]

attribs = {
    "mode": {"path": "mode", "type": str},
    "state": {"path": "state", "type": str},
    "n_buffers": {"path": "n_buffers", "type": int},
    "buffer_period_ns": {"path": "buffer_period_ns", "type": int},
    "samples_per_buffer": {"path": "samples_per_buffer", "type": int},
    "harvesting_voltage": {"path": "harvesting_voltage", "type": int},
    "mem_address": {"path": "memory/address", "type": int},
    "mem_size": {"path": "memory/size", "type": int},
}


def wait_for_state(state: str, timeout: float) -> NoReturn:
    """Waits until shepherd is in specified state.

    Polls the sysfs 'state' attribute until it contains the target state or
    until the timeout expires.

    Args:
        state (int): Target state
        timeout (float): Timeout in seconds
    """
    ts_start = time.time()
    while True:
        current_state = get_state()
        if current_state == state:
            return time.time() - ts_start

        if time.time() - ts_start > timeout:
            raise SysfsInterfaceException(
                (
                    f"timed out waiting for state { state } - "
                    f"state is { current_state }"
                )
            )
        time.sleep(0.1)


def set_start(start_time: float = None):
    """ Starts shepherd.

    Writes 'start' to the 'state' sysfs attribute in order to transition from
    'idle' to 'running' state. Optionally allows to start at a later point in
    time, transitioning shepherd to 'armed' state.

    Args:
        start_time (int): Desired start time in unix time
    """
    current_state = get_state()
    logger.debug(f"current state of shepherd kernel module: {current_state}")
    if current_state != "idle":
        raise SysfsInterfaceException(
            f"Cannot start from state { current_state }")

    with open(str(sysfs_path / "state"), "w") as f:
        if isinstance(start_time, float):
            start_time = int(start_time)
        if isinstance(start_time, int):
            logger.debug(f"writing start-time = {start_time} to sysfs")
            f.write(f"{start_time}")
        else:  # unknown type
            logger.debug(f"writing 'start' to sysfs")
            f.write("start")


def set_stop() -> NoReturn:
    """ Stops shepherd.

    Writes 'stop' to the 'state' sysfs attribute in order to transition from
    any state to 'idle'.
    """
    current_state = get_state()
    if current_state != "running":
        raise SysfsInterfaceException(
            f"Cannot stop from state { current_state }"
        )

    with open(str(sysfs_path / "state"), "w") as f:
        f.write("stop")


def set_mode(mode: str) -> NoReturn:
    """Sets the shepherd mode.

    Sets shepherd mode by writing corresponding string to the 'mode' sysfs
    attribute.

    Args:
        mode (str): Target mode. Must be one of harvesting, load, emulation or debug
    """
    if mode not in shepherd_modes:
        raise SysfsInterfaceException("invalid value for mode")
    if get_state() != "idle":
        raise SysfsInterfaceException(
            f"Cannot set mode when shepherd is { get_state() }"
        )

    logger.debug(f"mode: {mode}")
    with open(str(sysfs_path / "mode"), "w") as f:
        f.write(mode)


def set_harvesting_voltage(harvesting_voltage: int) -> NoReturn:
    """Sets the harvesting voltage.

    In some cases, it is necessary to fix the harvesting voltage, instead of
    relying on the built-in MPPT algorithm of the BQ25505. This function allows
    setting the set point by writing the desired value to the corresponding DAC.

    Args:
        harvesting_voltage (int): DAC value generating a fixed reference voltage
            on the reference input of BQ25505
    """
    if get_state() != "idle":
        raise SysfsInterfaceException(
            f"Cannot set voltage when shepherd state is { get_state() }"
        )
    mode = get_mode()
    if mode != "harvesting":
        raise SysfsInterfaceException(
            f"setting of harvesting voltage only possible in 'harvesting' mode"
        )
    with open(str(sysfs_path / "harvesting_voltage"), "w") as f:
        f.write(f"{ harvesting_voltage }")


def make_attr_getter(name: str, path: str, attr_type: type):
    """Instantiates a getter function for a sysfs attribute.

    To avoid hard-coding getter functions for all sysfs attributes, this
    function generates a getter function that also handles casting to the
    corresponding type

    Args:
        name (str): Name of the attribute
        path (str): Relative path of the attribute with respect to root
            shepherd sysfs path
        attr_type(type): Type of attribute, e.g. int or str
    """

    def _function():
        with open(str(sysfs_path / path), "r") as f:
            return attr_type(f.read().rstrip())

    return _function


# Automatically create getter for all attributes in props
for name, props in attribs.items():
    fun = make_attr_getter(name, props["path"], props["type"])
    setattr(sys.modules[__name__], f"get_{ name }", fun)
