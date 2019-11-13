import sys
import os
import numpy as np
from struct import unpack
from pathlib import Path

sys.path.append('../software/python-package')

from shepherd.shepherd_io import DataBuffer
from shepherd import CalibrationData
from shepherd import LogWriter
from shepherd import calibration_default

def random_data(len):
  return np.random.randint(0, high=2 ** 18, size=len, dtype="u4")

tmp_path = Path(os.getcwd())
filename = "solv3_4"

name = tmp_path / f"{filename}.h5"

current_offset = calibration_default.current_to_adc(0)
current_gain = calibration_default.current_to_adc(1) - current_offset
voltage_offset = calibration_default.voltage_to_adc(0)
voltage_gain = calibration_default.voltage_to_adc(1) - voltage_offset


current_list = []
voltage_list = []
length = 0

with open (f"{filename}.bin", 'rb') as f:
  while True:
    data = f.read(4)
    if len(data) < 4:
      break

    current, voltage = (unpack('HH', data))
    current_list.append(current / 4095 * 0.033 * current_gain + current_offset)
    voltage_list.append(voltage / 4095 * 3.3 * voltage_gain + voltage_offset)
    length = length + 1

with LogWriter(name, CalibrationData.from_default(), force=True) as store:
  data = DataBuffer(np.array(voltage_list), np.array(current_list), 0)
  store.write_buffer(data)

