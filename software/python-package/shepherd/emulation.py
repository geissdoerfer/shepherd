import logging
import time

from . import shepherd_io

logger = logging.getLogger(__name__)


class Emulator(shepherd_io.ShepherdIO):
    def __init__(
            self, calibration_recording, calibration_emulation,
            init_charge=False, load='node'):
        super().__init__('emulation', init_charge, load)

        self.transform_coeffs = {'voltage': dict(), 'current': dict()}
        for channel in ['voltage', 'current']:
            self.transform_coeffs[channel]['gain'] = (
                calibration_recording['harvesting'][channel]['gain']
                * calibration_emulation['emulation'][channel]['gain']
            )
            self.transform_coeffs[channel]['offset'] = (
                calibration_emulation['emulation'][channel]['gain']
                * calibration_recording['harvesting'][channel]['offset']
                + calibration_emulation['emulation'][channel]['offset']
            )

    def __enter__(self):
        super().__enter__()

        self.set_harvester(False)
        self.set_mppt(False)

        return self

    def put_buffer(self, index, buffer):

        ts_start = time.time()

        voltage_transformed = (
            buffer.voltage
            * self.transform_coeffs['voltage']['gain']
            + self.transform_coeffs['voltage']['offset']
        ).astype('u4')

        current_transformed = (
            buffer.current
            * self.transform_coeffs['current']['gain']
            + self.transform_coeffs['current']['offset']
        ).astype('u4')
    
        self.shared_mem.write_buffer(index, voltage_transformed, current_transformed)
        self.send_msg(shepherd_io.MSG_BUFFER_FROM_HOST, index)

        logger.debug((
            f"Returning buffer #{ index } to PRU took "
            f"{ time.time()-ts_start }")
        )