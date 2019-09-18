import sys

from datalog import LogReader
import calibration_default


input = sys.argv[1]

# "/home/borro0/mnt/var/rec.8.h5"

log_reader = LogReader(input, 10_000)
with log_reader:
  for hrvst_buf in log_reader.read_buffers(start=64):
    #take min and max values
    min_value = min(hrvst_buf.current)
    max_value = max(hrvst_buf.current)

    #convert to current
    min_value = calibration_default.adc_to_current(min_value)
    max_value = calibration_default.adc_to_current(max_value)

    print(f"min: {min_value}, max: {max_value}")