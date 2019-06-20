import logging
import numpy as np
import time

from . import shepherd_io
from . import sysfs_interface
from .datalog import DataBuffer

logger = logging.getLogger(__name__)


class Recorder(shepherd_io.ShepherdIO):
    def __init__(
            self, mode="harvesting", load="artificial",
            harvesting_voltage=None, init_charge=False):
        super().__init__(mode, init_charge, load)

        self.harvesting_voltage = harvesting_voltage


    def __enter__(self):
        super().__enter__()

        if self.harvesting_voltage is not None:
            self.set_mppt(False)
            sysfs_interface.set_harvesting_voltage(self.harvesting_voltage)
        else:
            self.set_mppt(True)

        self.set_harvester(True)

        if self.mode == "load":
            self.set_v_fixed(True)

        # Give the PRU empty buffers
        for i in range(self.n_buffers):
            self.send_msg(shepherd_io.MSG_BUFFER_FROM_HOST, i)
            logger.debug(f"sent empty buffer {i}")

        return self

    def put_buffer(self, index):
        logger.debug(f"Returning buffer #{ index } to PRU")
        self.send_msg(shepherd_io.MSG_BUFFER_FROM_HOST, index)
