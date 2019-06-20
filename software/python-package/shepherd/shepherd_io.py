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
from .datalog import DataBuffer

logger = logging.getLogger(__name__)

MSG_ERROR = 0
MSG_BUFFER_FROM_HOST = 1
MSG_BUFFER_FROM_PRU = 2

MSG_ERR_INCOMPLETE = 3
MSG_ERR_INVALIDCMD = 4
MSG_ERR_NOFREEBUF = 5

ID_ERR_TIMEOUT = 100

ADC_CH_V_IN = 0
ADC_CH_V_OUT = 1
ADC_CH_A_IN = 2
ADC_CH_A_OUT = 3

DAC_CH_V = 1
DAC_CH_A = 0

gpio_pin_nums = {
    'load': 48,
    'en_v_anlg': 60,
    'en_v_fix': 51,
    'en_mppt': 30,
    'en_hrvst': 23,
    'en_lvl_cnv': 80,
    'adc_rst_pdn': 88
}


class ShepherdIOException(Exception):
    def __init__(self, message, id=0, value=0):
        super().__init__(message)
        self.id = id
        self.value = value

class SharedMem(object):
    def __init__(self, address, size, n_buffers, samples_per_buffer):
        self.address = address
        self.size = size
        self.n_buffers = n_buffers
        self.samples_per_buffer = samples_per_buffer

        self.mapped_mem = None
        self.devmem_fd = None
    
        self.buffer_size = (
            12  # Header
            + 2 * 4 * self.samples_per_buffer  # Actual data
            + 4  # GPIO edge count
            + 4 * 16384 + 16384  # GPIO edge data
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


    def read_buffer(self, index):
        # The buffers are organized as an array in shared memory
        # buffer i starts at i * buffersize
        buffer_offset = index * self.buffer_size
        logger.debug(f"Seeking { index * self.buffer_size }")
        self.mapped_mem.seek(buffer_offset)

        # Read the header consisting of 12 (4 + 8 Bytes)
        header = self.mapped_mem.read(12)

        # First two values are number of samples and 64 bit timestamp
        n_samples, buffer_timestamp = struct.unpack("=LQ", header)
        logger.debug(f"Got buffer #{ index } with len {n_samples} and timestamp {buffer_timestamp}")

        # Each buffer contains samples_per_buffer values. We have 2 variables
        # (voltage and current), thus samples_per_buffer/2 samples per variable

        voltage_offset = buffer_offset + 12
        voltage = np.frombuffer(
            self.mapped_mem, "=u4",
            count=self.samples_per_buffer,
            offset=voltage_offset
        )

        current_offset = voltage_offset + 4 * self.samples_per_buffer
        current = np.frombuffer(
            self.mapped_mem,
            "=u4",
            count=self.samples_per_buffer,
            offset=current_offset
        )

        gpio_struct_offset = (
            buffer_offset
            + 12 # header
            + 2 * 4 * self.samples_per_buffer # current and voltage samples (4B)
        )
        # Jump over header and all sampled data
        self.mapped_mem.seek(gpio_struct_offset)
        # Read the number of gpio events in the buffer
        n_gpio_events, = struct.unpack("=L", self.mapped_mem.read(4))
        if n_gpio_events > 0:
            logger.info(f"Buffer contains {n_gpio_events} gpio events")

        gpio_ts_offset = gpio_struct_offset + 4
        gpio_time_offsets = np.frombuffer(
            self.mapped_mem,
            "=u4",
            count=n_gpio_events,
            offset=gpio_ts_offset
        )
        gpio_values_offset = gpio_ts_offset + 4 * (2**14)
        gpio_values = np.frombuffer(
            self.mapped_mem,
            "=u1",
            count=n_gpio_events,
            offset=gpio_values_offset
        )

        return DataBuffer(
            buffer_timestamp, self.samples_per_buffer, voltage, current,
            gpio_time_offsets, gpio_values)


    def write_buffer(self, index, voltage, current):

        buffer_offset = self.buffer_size * index
        # Seek buffer location in memory and skip 12B header
        self.mapped_mem.seek(buffer_offset + 12)
        self.mapped_mem.write(voltage)
        self.mapped_mem.write(current)



class ShepherdIO(object):
    """Generic ShepherdIO interface"""
    _instance = None

    def __new__(cls, *args, **kwds):

        if ShepherdIO._instance is None:
            new_class = object.__new__(cls)
            ShepherdIO._instance = weakref.ref(new_class)
        else:
            raise IndexError("ShepherdIO already exists")

        return new_class

    def __init__(self, mode, init_charge=False, load='artificial'):

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
                self.gpios[name] = GPIO(pin, 'out')

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
            self.n_buffers = sysfs_interface.get_n_buffers()

            self.shared_mem = SharedMem(
                mem_address, mem_size, self.n_buffers, samples_per_buffer)

            self.shared_mem.__enter__()

            logger.debug(f"Samples per buffer: { samples_per_buffer }")
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
        logger.info('exiting analog shepherd_io')
        self.cleanup()

    def send_msg(self, msg_type, value=0):
        msg = struct.pack("=II", msg_type, value)
        os.write(self.rpmsg_fd, msg)

    def get_msg(self, timeout=0.5):
        ts_end = time.time() + timeout
        while time.time() < ts_end:
            try:
                rep = os.read(self.rpmsg_fd, 8)
                return struct.unpack("=II", rep)
            except BlockingIOError:
                time.sleep(0.1)
                continue
        raise ShepherdIOException(
            "Timeout waiting for message", ID_ERR_TIMEOUT)

    def flush_msgs(self):
        while True:
            try:
                os.read(self.rpmsg_fd, 1)
            except BlockingIOError:
                break

    def start_sampling(self, start_time=None):
        sysfs_interface.start(start_time)

    def wait_for_start(self, timeout):
        sysfs_interface.wait_for_state('running', timeout)

    def cleanup(self):
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

    def set_power(self, state):
        self.gpios['en_v_anlg'].write(state)

    def set_mppt(self, state):
        self.gpios['en_mppt'].write(not state)

    def set_harvester(self, state):
        self.gpios['en_hrvst'].write(state)

    def set_v_fixed(self, state):
        self.gpios['en_v_fix'].write(state)

    def set_lvl_conv(self, state):
        self.gpios['en_lvl_cnv'].write(state)

    def set_load(self, load):
        if load.lower() == 'artificial':
            self.gpios['load'].write(False)
        elif load.lower() == 'node':
            self.gpios['load'].write(True)
        else:
            raise NotImplementedError('Load \"{}\" not supported'.format(load))

    def adc_set_power(self, state):
        self.gpios['adc_rst_pdn'].write(state)

    def get_buffer(self, timeout=1.0):
        msg_type, value = self.get_msg(timeout)

        if msg_type == MSG_BUFFER_FROM_PRU:
            logger.debug(f"Retrieving buffer { value } from shared memory")
            buf = self.shared_mem.read_buffer(value)
            return value, buf

        elif msg_type == MSG_ERR_INCOMPLETE:
            raise ShepherdIOException(
                "Got incomplete buffer", MSG_ERR_INCOMPLETE, value)

        elif msg_type == MSG_ERR_INVALIDCMD:
            raise ShepherdIOException(
                "PRU received invalid command", MSG_ERR_INVALIDCMD, value)
        elif msg_type == MSG_ERR_NOFREEBUF:
            raise ShepherdIOException(
                "PRU ran out of buffers", MSG_ERR_NOFREEBUF, value)
        else:
            raise ShepherdIOException(
                (f"Expected msg type { MSG_BUFFER_FROM_PRU } "
                 f"got { msg_type }[{ value }]")
            )




class ShepherdRawIO(ShepherdIO):
    CMD_ID_ADC = 0
    CMD_ID_DAC = 1

    # ADC Channel selection codes see ADS8694 datasheet Table 6
    adc_channel_codes = {0: 0xC000, 1: 0xC400, 2: 0xC800, 3: 0xCC00}

    """ Provides direct access to ADC and DAC without need for synchronization
        and kernel module. Used for GUI and calibration
    """
    def __init__(self):

        self.rpmsg_fd = None
        self.gpios = dict()

    def __enter__(self):

        try:
            for name, pin in gpio_pin_nums.items():
                self.gpios[name] = GPIO(pin, 'out')

            self.set_power(True)
            self.set_v_fixed(False)
            self.set_mppt(False)
            self.set_harvester(False)
            self.set_lvl_conv(False)

            self.adc_set_power(True)

            pru0.stop()
            pru0.set_firmware_name('am335x-pru0-debug-fw')

            # TODO: For some reason, PRU1 needs to start first
            pru0.start(retries=5)

            rpmsg_dev = Path("/dev/rpmsg_pru0")

            retries = 5
            while True:
                try:
                    self.rpmsg_fd = os.open(
                        str(rpmsg_dev), os.O_RDWR | os.O_SYNC)
                    break
                except FileNotFoundError:
                    if retries == 0:
                        raise
                    retries -= 1
                    time.sleep(2)

            os.set_blocking(self.rpmsg_fd, False)
        except Exception:
            self.cleanup()
            raise
        return self

    def adc_read(self, channel):
        if type(channel) is str:
            channel_no = getattr(
                sys.modules[__name__], f"ADC_CH_{ channel.upper() }")
        else:
            channel_no = channel

        try:
            channel_code = ShepherdRawIO.adc_channel_codes[channel_no]
        except KeyError as e:
            raise NotImplementedError(
                'ADC channel \"{}\" not supported'.format(channel_no)) from e
        self.send_msg(ShepherdRawIO.CMD_ID_ADC, channel_code)
        msg_id, value = self.get_msg()

        return value

    def dac_write(self, channel, value):
        if type(channel) is str:
            channel_no = getattr(
                sys.modules[__name__], f"DAC_CH_{ channel.upper() }")
        else:
            channel_no = channel

        if channel_no in [0, 1]:
            dac_command = value + (channel_no << 16)
            self.send_msg(ShepherdRawIO.CMD_ID_DAC, dac_command)
        else:
            raise NotImplementedError(
                'DAC channel \"{}\" not supported'.format(channel_no)
            )
        msg_id, value = self.get_msg()
        return value

    def cleanup(self):

        if self.rpmsg_fd:
            try:
                os.close(self.rpmsg_fd)
            except OSError:
                logger.warning("RPMSG file already closed")

        logger.debug("PRUs have been stopped")

        self.set_v_fixed(False)
        self.set_mppt(False)
        self.set_harvester(False)
        self.set_lvl_conv(False)
        self.adc_set_power(False)
        self.set_power(False)
        logger.debug("Analog shepherd_io is powered down")
