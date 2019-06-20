# Set default logging handler to avoid "No handler found" warnings.
import logging
from logging import NullHandler

from .recording import Recorder
from .emulation import Emulator
from .datalog import LogReader
from .datalog import LogWriter
from .eeprom import EEPROM
from .eeprom import CapeData
from .calibration import CalibrationData
from .shepherd_io import ShepherdRawIO
from .shepherd_io import ShepherdIOException

logging.getLogger(__name__).addHandler(NullHandler())
