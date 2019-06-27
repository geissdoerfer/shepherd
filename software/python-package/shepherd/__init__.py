import logging
import time
from logging import NullHandler

from .datalog import LogReader
from .datalog import LogWriter
from .eeprom import EEPROM
from .eeprom import CapeData
from .calibration import CalibrationData
from .shepherd_io import ShepherdIOException
from .shepherd_io import ShepherdIO
from . import commons

# Set default logging handler to avoid "No handler found" warnings.
logging.getLogger(__name__).addHandler(NullHandler())

logger = logging.getLogger(__name__)


class Recorder(ShepherdIO):
    """API for recording data with shepherd.

    Provides an easy to use, high-level interface for recording data with
    shepherd. Configures all hardware and initializes the communication
    with kernel module and PRUs.
    """

    def __init__(
        self,
        mode: str = "harvesting",
        load: str = "artificial",
        harvesting_voltage: int = None,
        init_charge: bool = False,
    ):
        """Inits super class and harvesting voltage.

        Args:
            mode (str): Should be either 'harvesting' to record harvesting data
                or 'load' to record target consumption data.
            load (str): Selects, which load should be used for recording.
                Should be one of 'artificial' or 'node'.
            harvesting_voltage (int): Allows to provide fixed reference voltage
                to boost converter. Binary DAC value.
            init_charge (bool): True to pre-charge the capacitor before starting
                recording.
        """
        super().__init__(mode, init_charge, load)

        self.harvesting_voltage = harvesting_voltage

    def __enter__(self):
        super().__enter__()

        if self.harvesting_voltage is not None:
            self.set_mppt(False)
            self.set_harvesting_voltage(self.harvesting_voltage)
        else:
            self.set_mppt(True)

        # In 'load' mode, the target is supplied from constant voltage reg
        if self.mode == "load":
            self.set_v_fixed(True)
            self.set_harvester(False)
        elif self.mode == "harvesting":
            self.set_harvester(True)

        # Give the PRU empty buffers to begin with
        for i in range(self.n_buffers):
            self.release_buffer(i)
            logger.debug(f"sent empty buffer {i}")

        return self


class Emulator(ShepherdIO):
    """API for emulating data with shepherd.

    Provides an easy to use, high-level interface for emulating data with
    shepherd. Configures all hardware and initializes the communication
    with kernel module and PRUs.
    """

    def __init__(
        self,
        calibration_recording: CalibrationData,
        calibration_emulation: CalibrationData,
        load: str = "node",
        init_charge: bool = False,
    ):
        """Inits super class and calculates transformation coefficients.

        Args:
            calibration_recording (CalibrationData): Shepherd calibration data
                belonging to the IV data that is being emulated
            calibration_emulation (CalibrationData): Shepherd calibration data
                belonging to the cape used for emulation
            load (str): Selects, which load should be used for recording.
                Should be one of 'artificial' or 'node'.
            init_charge (bool): True to pre-charge the capacitor before starting
                recording.
        """
        super().__init__("emulation", init_charge, load)

        # Values from recording are binary ADC values. We have to send binary
        # DAC values to the DAC for emulation. To directly convert ADC to DAC
        # values, we precalculate the 'transformation coefficients' based on
        # calibration data from the recorder and the emulator
        self.transform_coeffs = {"voltage": dict(), "current": dict()}
        for channel in ["voltage", "current"]:
            self.transform_coeffs[channel]["gain"] = (
                calibration_recording["harvesting"][channel]["gain"]
                * calibration_emulation["emulation"][channel]["gain"]
            )
            self.transform_coeffs[channel]["offset"] = (
                calibration_emulation["emulation"][channel]["gain"]
                * calibration_recording["harvesting"][channel]["offset"]
                + calibration_emulation["emulation"][channel]["offset"]
            )

    def __enter__(self):
        super().__enter__()

        # Disconnect harvester to avoid leakage in or out of the harvester
        self.set_harvester(False)
        # We will dynamically generate the reference voltage for the boost
        # converter. This only takes effect if MPPT is disabled.
        self.set_mppt(False)

        return self

    def put_buffer(self, index, buffer):

        ts_start = time.time()

        # Convert binary ADC recordings to binary DAC values
        voltage_transformed = (
            buffer.voltage * self.transform_coeffs["voltage"]["gain"]
            + self.transform_coeffs["voltage"]["offset"]
        ).astype("u4")

        current_transformed = (
            buffer.current * self.transform_coeffs["current"]["gain"]
            + self.transform_coeffs["current"]["offset"]
        ).astype("u4")

        self.shared_mem.write_buffer(
            index, voltage_transformed, current_transformed
        )
        self.release_buffer(index)

        logger.debug(
            (
                f"Returning buffer #{ index } to PRU took "
                f"{ time.time()-ts_start }"
            )
        )


class ShepherdDebug(ShepherdIO):
    """API for direct access to ADC and DAC.

    For debugging purposes, running the GUI or for retrieving calibration
    values, we need to directly read values from the ADC and set voltage using
    the DAC. This class allows to put the underlying PRUs and kernel module in
    a mode, where they accept 'debug messages' that allow to directly interface
    with the ADC and DAC.
    """

    def __init__(self):
        super().__init__("debug", False, "artificial")

    def adc_read(self, channel: str):
        """Reads value from specified ADC channel.

        Args:
            channel (str): Specifies the channel to read from, e.g., 'v_in' for
                harvesting voltage or 'i_out' for load current
        Returns:
            Binary ADC value read from corresponding channel
        """
        if channel.lower() == "v_in":
            channel_no = 0
        elif channel.lower() == "v_out":
            channel_no = 1
        elif channel.lower() in ["a_in", "i_in"]:
            channel_no = 2
        elif channel.lower() in ["a_out", "i_out"]:
            channel_no = 3
        else:
            raise ValueError(f"ADC channel { channel } unknown")

        self.send_msg(commons.MSG_DEP_DBG_ADC, channel_no)

        msg_type, value = self.get_msg()
        if msg_type != commons.MSG_DEP_DBG_ADC:
            raise ShepherdIOException(
                (
                    f"Expected msg type { commons.MSG_DEP_DBG_ADC } "
                    f"got { msg_type }[{ value }]"
                )
            )

        return value

    def dac_write(self, channel: str, value: int):
        """Writes value to specified DAC channel

        Args:
            channel (str): Specifies the channel to write to, e.g., 'current'
                for current channel or 'v' for voltage channel
            value (int): Binary DAC value to be sent to corresponding channel
        """
        # For a mapping of DAC channel to command refer to TI DAC8562T
        # datasheet Table 17
        if channel.lower() in ["current", "i", "a"]:
            dac_command = value
        elif channel.lower() in ["voltage", "v"]:
            # The DAC 'voltage' channel is on channel B
            dac_command = value | (1 << 16)
        else:
            raise ValueError(f"DAC channel { channel } unknown")

        self.send_msg(commons.MSG_DEP_DBG_DAC, dac_command)

    def get_buffer(self, timeout=None):
        raise NotImplementedError("Method not implemented for debugging mode")
