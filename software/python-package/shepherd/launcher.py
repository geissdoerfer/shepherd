# -*- coding: utf-8 -*-

"""
shepherd.launcher
~~~~~
Launcher allows to start and stop shepherd service with the press of a button.
Relies on systemd service.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

import dbus
import time
import logging
import os
from periphery import GPIO

logger = logging.getLogger(__name__)


class Launcher(object):
    """Stores data coming from PRU's in HDF5 format

    Args:
        pin_button (int): Pin number where button is connected. Must be
            configured as input with pull up and connected against ground
        pin_led (int): Pin number of LED for displaying launcher status
        service_name (str): Name of shepherd systemd service

    """

    def __init__(
        self,
        pin_button: int = 9,
        pin_led: int = 81,
        service_name: str = "shepherd",
    ):
        self.pin_button = pin_button
        self.pin_led = pin_led
        self.service_name = service_name

    def __enter__(self):
        self.gpio_led = GPIO(self.pin_led, "out")
        self.gpio_button = GPIO(self.pin_button, "in")
        self.gpio_button.edge = "falling"
        logger.debug("configured gpio")

        sysbus = dbus.SystemBus()
        systemd1 = sysbus.get_object(
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1"
        )
        self.manager = dbus.Interface(
            systemd1, "org.freedesktop.systemd1.Manager"
        )

        shepherd_object = self.manager.LoadUnit(
            f"{ self.service_name }.service"
        )
        self.shepherd_proxy = sysbus.get_object(
            "org.freedesktop.systemd1", str(shepherd_object)
        )
        logger.debug("configured dbus for systemd")

        return self

    def __exit__(self, *exc):
        self.gpio_led.close()
        self.gpio_button.close()

    def run(self):
        """Infinite loop waiting for button presses.

        Waits for falling edge on configured button pin. On detection of the
        edge, shepherd service is either started or stopped. Double button
        press while idle causes system shutdown.
        """
        while True:
            logger.info("waiting for edge..")
            self.gpio_led.write(True)
            self.gpio_button.poll()
            self.gpio_led.write(False)
            logger.debug("edge detected")
            if not self.get_state():
                time.sleep(0.25)
                if self.gpio_button.poll(timeout=5):
                    logging.debug("edge detected")
                    logging.info("shutdown requested")
                    self.initiate_shutdown()
                    self.gpio_led.write(False)
                    time.sleep(3)
                    continue
            self.set_service(not self.get_state())
            time.sleep(10)

    def get_state(self, timeout: float = 10):
        """Queries systemd for state of shepherd service.

        Args:
            timeout (float): Time to wait for service state to settle

        Raises:
            TimeoutError: If state remains changing for longer than timeout
        """
        ts_end = time.time() + timeout

        while True:
            systemd_state = self.shepherd_proxy.Get(
                "org.freedesktop.systemd1.Unit",
                "ActiveState",
                dbus_interface="org.freedesktop.DBus.Properties",
            )
            if systemd_state in ["deactivating", "activating"]:
                time.sleep(0.1)
                continue
            else:
                break
            if time.time() > ts_end:
                raise TimeoutError("Timed out waiting for service state")

        logger.debug(f"service ActiveState: { systemd_state }")

        if systemd_state == "active":
            return True
        elif systemd_state == "inactive":
            return False
        raise Exception(f"Unknown state { systemd_state }")

    def set_service(self, requested_state: str):
        """Changes state of shepherd service.

        Args:
            requested_state (str): Target state of service
        """
        active_state = self.get_state()

        if requested_state == active_state:
            logger.debug("service already in requested state")
            self.gpio_led.write(active_state)
            return

        if active_state:
            logger.info("stopping service")
            self.manager.StopUnit("shepherd.service", "fail")
        else:
            logger.info("starting service")
            self.manager.StartUnit("shepherd.service", "fail")

        time.sleep(1)

        new_state = self.get_state()
        if new_state != requested_state:
            raise Exception(f"state didn't change")

        return new_state

    def initiate_shutdown(self, timeout: int = 5):
        """Initiates system shutdown.

        Args:
            timeout (int): Number of seconds to wait before powering off
                system
        """
        logger.debug("initiating shutdown routine..")
        time.sleep(0.25)
        for _ in range(timeout):
            if self.gpio_button.poll(timeout=0.5):
                logger.debug("edge detected")
                logger.info("shutdown cancelled")
                return
            self.gpio_led.write(True)
            if self.gpio_button.poll(timeout=0.5):
                logger.debug("edge detected")
                logger.info("shutdown cancelled")
                return
            self.gpio_led.write(False)
        os.sync()
        logger.info("shutting down now")
        self.manager.PowerOff()
