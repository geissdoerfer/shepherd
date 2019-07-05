# -*- coding: utf-8 -*-

"""
shepherd.shepherd_io
~~~~~
Interface layer, abstracting low-level functionality provided by PRUs and
kernel module. User-space part of the double-buffered data exchange protocol.


:copyright: (c) 2019 by Kai Geissdoerfer.
:license: MIT, see LICENSE for more details.
"""

import os
import weakref
import logging
import time
import atexit
import struct
import mmap
import sys
import numpy as np
from pathlib import Path
from periphery import GPIO

from . import sysfs_interface
from . import commons

logger = logging.getLogger(__name__)

ID_ERR_TIMEOUT = 100

gpio_pin_nums = {
    "load": 48,
    "en_v_anlg": 60,
    "en_v_fix": 51,
    "en_mppt": 30,
    "en_hrvst": 23,
    "en_lvl_cnv": 80,
    "adc_rst_pdn": 88,
}


class ShepherdIOException(Exception):
    def __init__(self, message: str, id: int = 0, value: int = 0):
        super().__init__(message)
        self.id = id
        self.value = value


class GPIOEdges(object):
    """Python representation of GPIO edge buffer
    
    On detection of an edge, shepherd stores the state of all sampled GPIO pins
    together with the corresponding timestamp
    """

    def __init__(
        self, timestamps_ns: np.ndarray = None, values: np.ndarray = None
    ):
        if timestamps_ns is None:
            self.timestamps_ns = np.empty(0)
            self.values = np.empty(0)
        else:
            self.timestamps_ns = timestamps_ns
            self.values = values

    def __len__(self):
        return self.values.size


class DataBuffer(object):
    """Python representation of a shepherd buffer.
    
    Containing IV samples with corresponding timestamp and info about any
    detected GPIO edges
    """

    def __init__(
        self,
        voltage: np.ndarray,
        current: np.ndarray,
        timestamp_ns: int = None,
        gpio_edges: GPIOEdges = None,
    ):
        self.timestamp_ns = timestamp_ns
        self.voltage = voltage
        self.current = current
        if gpio_edges is not None:
            self.gpio_edges = gpio_edges
        else:
            self.gpio_edges = GPIOEdges()

    def __len__(self):
        return self.voltage.size


class SharedMem(object):
    """Represents shared RAM used to exchange data between PRUs and userspace.

    A large area of contiguous memory is allocated through remoteproc. The PRUs
    have access to this memory and store/retrieve IV data from this area. It is
    one of the two key components in the double-buffered data exchange protocol.
    The userspace application has to map this memory area into its own memory
    space. This is achieved through /dev/mem which allow to map physical memory
    locations into userspace under linux.
    """

    def __init__(
        self, address: int, size: int, n_buffers: int, samples_per_buffer: int
    ):
        """Initializes relevant parameters for shared memory area.

        Args:
            address (int): Physical start address of memory area
            size (int): Total size of memory area in Byte
            n_buffers (int): Number of data buffers that fit into memory area
            samples_per_buffer (int): Number of IV samples per buffer
        """
        self.address = address
        self.size = size
        self.n_buffers = n_buffers
        self.samples_per_buffer = samples_per_buffer

        self.mapped_mem = None
        self.devmem_fd = None

        # With knowledge of structure of each buffer, we calculate its total size
        self.buffer_size = (
            # Header: 8B timestamp + 4B size
            12
            # Actual IV data, 4B for each current and voltage
            + 2 * 4 * self.samples_per_buffer
            # GPIO edge count
            + 4
            # 8B timestamp per GPIO event
            + 8 * commons.MAX_GPIO_EVT_PER_BUFFER
            # 1B GPIO state per GPIO event
            + commons.MAX_GPIO_EVT_PER_BUFFER  # GPIO edge data
        )

        logger.debug(f"Individual buffer size: { self.buffer_size }B")

    def __enter__(self):
        self.devmem_fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)

        self.mapped_mem = mmap.mmap(
            self.devmem_fd,
            self.size,
            mmap.MAP_SHARED,
            mmap.PROT_WRITE,
            offset=self.address,
        )

        return self

    def __exit__(self, *args):
        self.mapped_mem.close()
        os.close(self.devmem_fd)

    def read_buffer(self, index: int):
        """Extracts buffer from shared memory.

        Extracts data from buffer with given index from the shared memory area
        in RAM.

        Args:
            index (int): Buffer index. 0 <= index < n_buffers
        
        Returns:
            DataBuffer object pointing to extracted data
        """
        # The buffers are organized as an array in shared memory
        # buffer i starts at i * buffersize
        buffer_offset = index * self.buffer_size
        logger.debug(f"Seeking 0x{index * self.buffer_size:04X}")
        self.mapped_mem.seek(buffer_offset)

        # Read the header consisting of 12 (4 + 8 Bytes)
        header = self.mapped_mem.read(12)

        # First two values are number of samples and 64 bit timestamp
        n_samples, buffer_timestamp = struct.unpack("=LQ", header)
        logger.debug(
            f"Got buffer #{ index } with len {n_samples} and timestamp {buffer_timestamp}"
        )

        # Each buffer contains samples_per_buffer values. We have 2 variables
        # (voltage and current), thus samples_per_buffer/2 samples per variable

        voltage_offset = buffer_offset + 12
        voltage = np.frombuffer(
            self.mapped_mem,
            "=u4",
            count=self.samples_per_buffer,
            offset=voltage_offset,
        )

        current_offset = voltage_offset + 4 * self.samples_per_buffer
        current = np.frombuffer(
            self.mapped_mem,
            "=u4",
            count=self.samples_per_buffer,
            offset=current_offset,
        )

        gpio_struct_offset = (
            buffer_offset
            + 12  # header
            + 2
            * 4
            * self.samples_per_buffer  # current and voltage samples (4B)
        )
        # Jump over header and all sampled data
        self.mapped_mem.seek(gpio_struct_offset)
        # Read the number of gpio events in the buffer
        n_gpio_events, = struct.unpack("=L", self.mapped_mem.read(4))
        if n_gpio_events > 0:
            logger.info(f"Buffer contains {n_gpio_events} gpio events")

        gpio_ts_offset = gpio_struct_offset + 4
        gpio_timestamps_ns = np.frombuffer(
            self.mapped_mem, "=u8", count=n_gpio_events, offset=gpio_ts_offset
        )
        gpio_values_offset = (
            gpio_ts_offset + 8 * commons.MAX_GPIO_EVT_PER_BUFFER
        )
        gpio_values = np.frombuffer(
            self.mapped_mem,
            "=u1",
            count=n_gpio_events,
            offset=gpio_values_offset,
        )
        gpio_edges = GPIOEdges(gpio_timestamps_ns, gpio_values)

        return DataBuffer(voltage, current, buffer_timestamp, gpio_edges)

    def write_buffer(self, index, voltage, current):

        buffer_offset = self.buffer_size * index
        # Seek buffer location in memory and skip 12B header
        self.mapped_mem.seek(buffer_offset + 12)
        self.mapped_mem.write(voltage)
        self.mapped_mem.write(current)


class ShepherdIO(object):
    """Generic ShepherdIO interface.
    
    This class acts as interface between kernel module and firmware on the PRUs,
    and user space code. It handles the user space part of the double-buffered
    data-exchange protocol between user space and PRUs and configures the
    hardware by setting corresponding GPIO pins. This class should usually not
    be instantiated, but instead serve as parent class for e.g. Recorder or
    Emulator (see __init__.py).
    """

    # This is part of the singleton implementation
    _instance = None

    def __new__(cls, *args, **kwds):
        """Implements singleton class."""
        if ShepherdIO._instance is None:
            new_class = object.__new__(cls)
            ShepherdIO._instance = weakref.ref(new_class)
        else:
            raise IndexError("ShepherdIO already exists")

        return new_class

    def __init__(
        self, mode: str, init_charge: bool = False, load: str = "artificial"
    ):
        """Initializes relevant variables.

        Args:
            mode (str): Shepherd mode, one of 'harvesting', 'load', 'emulation'
            init_charge (bool): Specifies whether capacitor should be charged
                initially
            load (str): Which load to use, one of 'artificial', 'node'
        """

        self.rpmsg_fd = None
        self.mode = mode
        self.gpios = dict()
        self.init_charge = init_charge
        self.load = load
        self.shared_mem = None

    def __del__(self):
        ShepherdIO._instance = None

    def __enter__(self):
        try:

            for name, pin in gpio_pin_nums.items():
                self.gpios[name] = GPIO(pin, "out")

            self.set_power(True)
            self.set_v_fixed(False)
            self.set_mppt(False)
            self.set_harvester(False)
            self.set_lvl_conv(False)

            self.adc_set_power(True)

            logger.debug("Analog shepherd_io is powered")

            # Allow PRUs some time to setup RPMSG and sync
            sysfs_interface.wait_for_state("idle", 5)
            logger.debug(f"Switching to '{ self.mode }' mode")
            sysfs_interface.set_mode(self.mode)

            # Open the RPMSG channel provided by rpmsg_pru kernel module
            rpmsg_dev = Path("/dev/rpmsg_pru0")
            self.rpmsg_fd = os.open(str(rpmsg_dev), os.O_RDWR | os.O_SYNC)
            os.set_blocking(self.rpmsg_fd, False)
            self.flush_msgs()

            # Ask PRU for base address of shared mem (reserved with remoteproc)
            mem_address = sysfs_interface.get_mem_address()
            # Ask PRU for length of shared memory (reserved with remoteproc)
            mem_size = sysfs_interface.get_mem_size()

            logger.debug(
                f"Shared memory address: {mem_address:08X} length: {mem_size}"
            )

            # Ask PRU for size of individual buffers
            samples_per_buffer = sysfs_interface.get_samples_per_buffer()
            logger.debug(f"Samples per buffer: { samples_per_buffer }")

            self.n_buffers = sysfs_interface.get_n_buffers()
            logger.debug(f"Number of buffers: { self.n_buffers }")

            self.buffer_period_ns = sysfs_interface.get_buffer_period_ns()
            logger.debug(f"Buffer period: { self.buffer_period_ns }ns")

            self.shared_mem = SharedMem(
                mem_address, mem_size, self.n_buffers, samples_per_buffer
            )

            self.shared_mem.__enter__()

            logger.debug(f"Setting load to '{ self.load }'")
            self.set_load(self.load)

            if self.init_charge:
                logger.debug("pre-charging capacitor")
                self.set_v_fixed(True)
                time.sleep(1.0)
                self.set_v_fixed(False)

        except Exception:
            self.cleanup()
            raise
        return self

    def __exit__(self, *args):
        logger.info("exiting analog shepherd_io")
        self.cleanup()

    def send_msg(self, msg_type: int, value: int):
        """Sends a formatted message to PRU0 via rpmsg channel.
        
        Args:
            msg_type (int): Indicates type of message, must be one of the agreed
                message types part of the data exchange protocol
            value (int): Actual content of the message
        """
        msg = struct.pack("=II", msg_type, value)
        os.write(self.rpmsg_fd, msg)

    def get_msg(self, timeout: float = 0.5):
        """Tries to retrieve formatted message from PRU0 via rpmsg channel.

        Args:
            timeout (float): Maximum number of seconds to wait for a message
                before raising timeout exception
        """
        ts_end = time.time() + timeout
        while time.time() < ts_end:
            try:
                rep = os.read(self.rpmsg_fd, 8)
                return struct.unpack("=II", rep)
            except BlockingIOError:
                time.sleep(0.1)
                continue
        raise ShepherdIOException(
            "Timeout waiting for message", ID_ERR_TIMEOUT
        )

    def flush_msgs(self):
        """Flushes rpmsg channel by reading all available bytes."""
        while True:
            try:
                os.read(self.rpmsg_fd, 1)
            except BlockingIOError:
                break

    def start_sampling(self, start_time: int = None):
        """Starts sampling either now or at later point in time.

        Args:
            start_time (int): Desired start time in unix time
        """
        sysfs_interface.start(start_time)

    def wait_for_start(self, timeout: float):
        """Waits until shepherd has started sampling.

        Args:
            timeout (float): Time to wait in seconds
        """
        sysfs_interface.wait_for_state("running", timeout)

    def cleanup(self):

        try:
            sysfs_interface.stop()
        except Exception:
            pass

        if self.shared_mem is not None:
            self.shared_mem.__exit__()

        if self.rpmsg_fd is not None:
            os.close(self.rpmsg_fd)

        self.set_v_fixed(False)
        self.set_mppt(False)
        self.set_harvester(False)
        self.set_lvl_conv(False)
        self.adc_set_power(False)
        self.set_power(False)
        logger.debug("Analog shepherd_io is powered down")

    def set_power(self, state: bool):
        """Controls state of main analog power supply on shepherd cape.

        Args:
            state (bool): True for on, False for off
        """
        self.gpios["en_v_anlg"].write(state)

    def set_mppt(self, state: bool):
        """Enables or disables Maximum Power Point Tracking of BQ25505

        TI's BQ25505 implements an MPPT algorithm, that dynamically adapts
        the harvesting voltage according to current conditions. Alternatively,
        it allows to provide a reference voltage to which it will regulate the
        harvesting voltage. This is necessary for emulation and can be used to
        fix harvesting voltage during recording as well.

        Args:
            state (bool): True for enabling MPPT, False for disabling
        """
        self.gpios["en_mppt"].write(not state)

    def set_harvester(self, state: bool):
        """Enables or disables connection to the harvester.

        The harvester is connected to the main power path of the shepherd cape
        through a MOSFET relay that allows to make or break that connection.
        This way, we can completely disable the harvester during emulation.

        Args:
            state (bool): True for enabling harvester, False for disabling
        """
        self.gpios["en_hrvst"].write(state)

    def set_v_fixed(self, state: bool):
        """Enables or disables the constant voltage regulator.

        The shepherd cape has a linear regulator that is connected to the load
        power path through a diode. This allows to pre-charge the capacitor to
        a defined value or to supply a sensor node with a fixed voltage. This
        function allows to enable or disable the output of this regulator.

        Args:
            state (bool): True for enabling regulator, False for disabling
        """
        self.gpios["en_v_fix"].write(state)

    def set_lvl_conv(self, state: bool):
        """Enables or disables the GPIO level converter.

        The shepherd cape has a bi-directional logic level shifter (TI TXB0304)
        for translating UART and SWD signals between BeagleBone and target
        voltage levels. This function enables or disables the converter.

        Args:
            state (bool): True for enabling converter, False for disabling
        """
        self.gpios["en_lvl_cnv"].write(state)

    def set_load(self, load: str):
        """Selects which load is connected to shepherd's output.

        The output of the main power path can be connected either to the
        on-board 'artificial' load (for recording) or to an attached sensor
        node (for emulation). This function configures the corresponding
        hardware switch.

        Args:
            load (str): The load to connect to the output of shepherd's output.
            One of 'artificial' or 'node'.
        
        Raises:
            NotImplementedError: If load is not 'artificial' or 'node'
        """
        if load.lower() == "artificial":
            self.gpios["load"].write(False)
        elif load.lower() == "node":
            self.gpios["load"].write(True)
        else:
            raise NotImplementedError('Load "{}" not supported'.format(load))

    def adc_set_power(self, state: bool):
        """Controls power/reset of shepherd's ADC.

        The TI ADS8694 has a power/reset pin, that can be used to power down
        the device or perform a hardware reset. This function controls the
        state of the pin.

        Args:
            state (bool): True for 'on', False for 'off'
        """
        self.gpios["adc_rst_pdn"].write(state)

    def set_harvesting_voltage(self, dac_value: int):
        """Sets the reference voltage for the boost converter

        In some cases, it is necessary to fix the harvesting voltage, instead
        of relying on the built-in MPPT algorithm of the BQ25505. This function
        allows setting the set point by writing the desired value to the
        corresponding DAC. Note that the setting only takes effect, if MPPT
        is disabled (see set_mppt())

        Args:
            dac_value (int): Value to be written to DAC
        """
        sysfs_interface.set_harvesting_voltage(dac_value)

    def release_buffer(self, index: int):
        """Returns a buffer to the PRU

        After reading the content of a buffer and potentially filling it with
        emulation data, we have to release the buffer to the PRU to avoid it
        running out of buffers.

        Args:
            index (int): Index of the buffer. 0 <= index < n_buffers
        """

        logger.debug(f"Releasing buffer #{ index } to PRU")
        self.send_msg(commons.MSG_DEP_BUF_FROM_HOST, index)

    def get_buffer(self, timeout: float = 1.0):
        """Reads a data buffer from shared memory.

        Polls the RPMSG channel for a message from PRU0 and, if the message
        points to a filled buffer in memory, returns the data in the
        corresponding memory location as DataBuffer.

        Args:
            timeout (float): Time in seconds that should be waited for an
                incoming RPMSG
        Returns:
            Index and content of corresponding data buffer
        Raises:
            TimeoutException: If no message is received on RPMSG within
                specified timeout

        """
        msg_type, value = self.get_msg(timeout)

        if msg_type == commons.MSG_DEP_BUF_FROM_PRU:
            logger.debug(f"Retrieving buffer { value } from shared memory")
            buf = self.shared_mem.read_buffer(value)
            return value, buf

        elif msg_type == commons.MSG_DEP_ERR_INCMPLT:
            raise ShepherdIOException(
                "Got incomplete buffer", commons.MSG_DEP_ERR_INCMPLT, value
            )

        elif msg_type == commons.MSG_DEP_ERR_INVLDCMD:
            raise ShepherdIOException(
                "PRU received invalid command",
                commons.MSG_DEP_ERR_INVLDCMD,
                value,
            )
        elif msg_type == commons.MSG_DEP_ERR_NOFREEBUF:
            raise ShepherdIOException(
                "PRU ran out of buffers", commons.MSG_DEP_ERR_NOFREEBUF, value
            )
        else:
            raise ShepherdIOException(
                (
                    f"Expected msg type { commons.MSG_DEP_BUF_FROM_PRU } "
                    f"got { msg_type }[{ value }]"
                )
            )
