import logging
import time
import numpy as np
from pathlib import Path
import h5py
from itertools import product
from collections import defaultdict
from collections import namedtuple

from shepherd.calibration import CalibrationData

logger = logging.getLogger(__name__)

ExceptionRecord = namedtuple(
    "ExceptionRecord", ["timestamp", "message", "value"])


class DataBuffer(object):
    def __init__(self, timestamp, buffer_len, voltage=None, current=None, gpio_time_offsets=None, gpio_values=None):
        self.timestamp = timestamp
        self.buffer_len = buffer_len
        self.voltage = voltage
        self.current = current

        if gpio_time_offsets is None:
            self.gpio_time_offsets = np.empty((0,))
            self.gpio_values = np.empty((0,))
        else:
            self.gpio_time_offsets = gpio_time_offsets
            self.gpio_values = gpio_values


class LogWriter(object):
    "Stores data coming from PRU's in HDF5 format"

    complevel = 1
    compalg = "lzf"

    def __init__(
        self,
        store_name,
        calibration_data: CalibrationData,
        mode="harvesting",
        force=False,
    ):
        if force or not Path(store_name).exists():
            self.store_name = store_name
        else:
            raise FileExistsError(f"Measurement {store_name} already exists")
        self.mode = mode
        self.calibration_data = calibration_data

        if mode == "both":
            self.chunk_shape = (5_000,)
        else:
            self.chunk_shape = (10_000,)


    def __enter__(self):
        self._h5file = h5py.File(self.store_name, "w")

        self.data_grp = self._h5file.create_group("data")
        self.data_grp.create_dataset(
            "time", (0,), dtype="u8", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.data_grp["time"].attrs["unit"] = f"system time in nano seconds"

        # Create group for exception logs
        self.log_grp = self._h5file.create_group("log")
        self.log_grp.create_dataset(
            "time", (0,), dtype="u8", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.log_grp["time"].attrs["unit"] = f"system time in nano seconds"
        self.log_grp.create_dataset(
            "message", (0,), dtype=h5py.special_dtype(vlen=str),
            maxshape=(None,)
        )
        self.log_grp.create_dataset(
            "value", (0,), dtype="u4", maxshape=(None,),
        )

        # Create group for gpio data
        self.gpio_grp = self._h5file.create_group("gpio")
        self.gpio_grp.create_dataset(
            "time", (0,), dtype="u8", maxshape=(None,),
            compression=LogWriter.compalg
        )
        self.gpio_grp["time"].attrs["unit"] = f"system time in nano seconds"
        self.gpio_grp.create_dataset(
            "value", (0,), dtype="u1", maxshape=(None,),
            compression=LogWriter.compalg
        )

        if self.mode == "harvesting":
            self.create_harvesting_group()
            self.store_calibration("harvesting")
        if self.mode == "load":
            self.create_load_group()
            self.store_calibration("load")
        return self

    def __exit__(self, *exc):
        logger.info("flushing and closing hdf5 file")
        self._h5file.flush()
        self._h5file.close()

    def write_data(self, buffer: DataBuffer):
        "Writes data from buffer to file"
        buffer_duration = 100_000_000

        buffer_len = buffer.buffer_len
        sampling_interval = int(buffer_duration / buffer_len)
        current_length = self.data_grp["time"].shape[0]
        new_length = current_length + buffer_len

        self.data_grp["time"].resize((new_length,))
        self.data_grp["time"][current_length:] = (
            buffer.timestamp
            + sampling_interval * np.arange(buffer_len)
        )

        for variable in ["voltage", "current"]:
            self.data_grp[self.mode][variable].resize((new_length,))
            self.data_grp[self.mode][variable][current_length:] = getattr(
                buffer, variable)

        if len(buffer.gpio_time_offsets) > 0:
            # The offsets are in IEP ticks (5ns)
            gpio_times = buffer.timestamp + buffer.gpio_time_offsets * 5
            gpio_current_length = self.gpio_grp["time"].shape[0]
            gpio_new_length = (
                gpio_current_length + len(buffer.gpio_time_offsets)
            )
            self.gpio_grp["time"].resize((gpio_new_length,))
            self.gpio_grp["value"].resize((gpio_new_length,))
            self.gpio_grp["time"][gpio_current_length:] = gpio_times
            self.gpio_grp["value"][gpio_current_length:] = buffer.gpio_values

    def write_exception(self, exception):
        current_length = self.log_grp["time"].shape[0]
        self.log_grp["time"].resize((current_length + 1,))
        self.log_grp["time"][current_length] = exception.timestamp
        self.log_grp["value"].resize((current_length + 1,))
        self.log_grp["value"][current_length] = exception.value
        self.log_grp["message"].resize((current_length + 1,))
        self.log_grp["message"][current_length] = exception.message

    def create_harvesting_group(self):
        self.harvesting_grp = self._h5file["data"].create_group("harvesting")
        self.harvesting_grp.create_dataset(
            "current", (0,), dtype="u4", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.harvesting_grp["current"].attrs["unit"] = "A"
        self.harvesting_grp.create_dataset(
            "voltage", (0,), dtype="u4", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.harvesting_grp["voltage"].attrs["unit"] = "V"

    def create_load_group(self):
        self.load_grp = self._h5file["data"].create_group("load")
        self.load_grp.create_dataset(
            "current", (0,), dtype="u4", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.load_grp["current"].attrs["unit"] = "A"
        self.load_grp.create_dataset(
            "voltage", (0,), dtype="u4", maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg
        )
        self.load_grp["voltage"].attrs["unit"] = "V"

    def __setitem__(self, key, item):
        return self._h5file.attrs.__setitem__(key, item)

    def store_calibration(self, mode):
        """

            Calibration data is in the following form:

            ```
            harvesting:
            current:
                gain: 3.822167808088151e-07
                offset: -0.050097855530474036
            voltage:
                gain: 1.953125e-05
                offset: 0.0
            ```
        """
        for variable, attr in product(
                ["voltage", "current"], ["gain", "offset"]):
            self.data_grp[mode][variable].attrs[attr] = (
                self.calibration_data[mode][variable][attr]
            )


class LogReader(object):
    def __init__(self, store_name, block_len):
        self.store_name = store_name
        self._block_len = block_len

    def __enter__(self):
        self._h5file = h5py.File(self.store_name, "r")
        self.harvesting_voltage = (
            self._h5file["data"]["harvesting"]["voltage"]
        )
        self.harvesting_current = (
            self._h5file["data"]["harvesting"]["current"]
        )
        return self

    def __exit__(self, *exc):
        self._h5file.close()

    def read_blocks(self, start=0, end=None):
        if end is None:
            end = int(
                self._h5file["data"]["time"].shape[0] / self._block_len)
        logger.debug(f"Reading blocks from { start } to { end } from log")

        for i in range(start, end):
            ts_start = time.time()
            idx_start = i * self._block_len
            idx_end = (i + 1) * self._block_len
            db = DataBuffer(
                timestamp=None,
                buffer_len=None,
                voltage=self.harvesting_voltage[idx_start:idx_end],
                current=self.harvesting_current[idx_start:idx_end],
            )
            logger.debug(
                (
                    f"Reading datablock with {self._block_len} samples "
                    f"from netcdf took { time.time()-ts_start }"
                )
            )
            yield db

    def get_calibration_data(self):
        """Retrieves calibration data from all of the groups
        """
        nested_dict = lambda: defaultdict(nested_dict)
        calib = nested_dict()
        for group, var, attr in product(
            ["harvesting", "load", "emulation"], ["voltage", "current"],
            ["gain", "offset"]
        ):
            if group not in self._h5file["data"].keys():
                continue
            logger.debug(f"{group}  {var} {attr}")
            calib[group][var][attr] = \
                self._h5file["data"][group][var].attrs[attr]
        return CalibrationData(calib)
