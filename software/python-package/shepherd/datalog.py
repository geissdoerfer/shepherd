# -*- coding: utf-8 -*-

"""
shepherd.datalog
~~~~~
Provides classes for storing and retrieving sampled IV data to/from
HDF5 files.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""

import logging
import time
import numpy as np
from pathlib import Path
import h5py
from itertools import product
from collections import defaultdict
from collections import namedtuple

from shepherd.calibration import CalibrationData
from shepherd.shepherd_io import DataBuffer
from shepherd.shepherd_io import GPIOEdges

logger = logging.getLogger(__name__)

"""
An entry for an exception to be stored together with the data consists of a
timestamp, a custom message and an arbitrary integer value
"""
ExceptionRecord = namedtuple(
    "ExceptionRecord", ["timestamp", "message", "value"]
)


def unique_path(base_path: str, suffix: str):
    counter = 0
    while True:
        path = base_path.with_suffix(f".{ counter }{ suffix }")
        if not path.exists():
            return path
        counter += 1


class LogWriter(object):
    """Stores data coming from PRU's in HDF5 format

    Args:
        store_path (str): Name of the HDF5 file that data will be written to
        calibration_data (CalibrationData): Data is written as raw ADC
            values. We need calibration data in order to convert to physical
            units later.
        mode (str): Indicates if this is data from recording or emulation
        force (bool): Overwrite existing file with the same name
        samples_per_buffer (int): Number of samples contained in a single
            shepherd buffer
        buffer_period_ns (int): Duration of a single shepherd buffer in
            nanoseconds

    """

    complevel = 1
    compalg = "lzf"

    def __init__(
        self,
        store_path: Path,
        calibration_data: CalibrationData,
        mode: str = "harvesting",
        force: bool = False,
        samples_per_buffer: int = 10_000,
        buffer_period_ns: int = 100_000_000,
    ):
        if force or not store_path.exists():
            self.store_path = store_path
        else:
            base_dir = store_path.resolve().parents[0]
            self.store_path = unique_path(
                base_dir / store_path.stem, store_path.suffix
            )
            logger.warning(
                (
                    f"File {str(store_path)} already exists.. "
                    f"storing under {str(self.store_path)} instead"
                )
            )
        self.mode = mode
        self.calibration_data = calibration_data
        self.chunk_shape = (samples_per_buffer,)
        self.sampling_interval = int(buffer_period_ns / samples_per_buffer)

    def __enter__(self):
        """Initializes the structure of the HDF5 file

        HDF5 is hierarchically structured and before writing data, we have to
        setup this structure, i.e. creating the right groups with corresponding
        data types. We will store 3 types of data in a LogWriter database: The
        actual IV samples recorded either from the harvester (during recording)
        or the load (during emulation). Any log messages, that can be used to
        store relevant events or tag some parts of the recorded data. And lastly
        the state of the GPIO pins.

        """
        self._h5file = h5py.File(self.store_path, "w")

        # Store the mode in order to allow user to differentiate harvesting vs load data
        self._h5file.attrs.__setitem__("mode", self.mode)

        # Store voltage and current samples in the data group
        self.data_grp = self._h5file.create_group("data")
        # Timestamps in ns are stored as 8 Byte unsigned integer
        self.data_grp.create_dataset(
            "time",
            (0,),
            dtype="u8",
            maxshape=(None,),
            # This makes writing more efficient, see HDF5 docs
            chunks=self.chunk_shape,
            compression=LogWriter.compalg,
        )
        self.data_grp["time"].attrs["unit"] = f"system time in nano seconds"

        # Both current and voltage are stored as 4 Byte unsigned int
        self.data_grp.create_dataset(
            "current",
            (0,),
            dtype="u4",
            maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg,
        )
        self.data_grp["current"].attrs["unit"] = "A"
        self.data_grp.create_dataset(
            "voltage",
            (0,),
            dtype="u4",
            maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg,
        )
        self.data_grp["voltage"].attrs["unit"] = "V"
        # Refer to shepherd/calibration.py for the format of calibration data
        for variable, attr in product(
            ["voltage", "current"], ["gain", "offset"]
        ):
            self.data_grp[variable].attrs[attr] = self.calibration_data[
                self.mode
            ][variable][attr]

        # Create group for exception logs
        self.log_grp = self._h5file.create_group("log")
        self.log_grp.create_dataset(
            "time",
            (0,),
            dtype="u8",
            maxshape=(None,),
            chunks=self.chunk_shape,
            compression=LogWriter.compalg,
        )
        self.log_grp["time"].attrs["unit"] = f"system time in nano seconds"
        # Every log entry consists of a timestamp, a message and a value
        self.log_grp.create_dataset(
            "message",
            (0,),
            dtype=h5py.special_dtype(vlen=str),
            maxshape=(None,),
        )
        self.log_grp.create_dataset("value", (0,), dtype="u4", maxshape=(None,))

        # Create group for gpio data
        self.gpio_grp = self._h5file.create_group("gpio")
        self.gpio_grp.create_dataset(
            "time",
            (0,),
            dtype="u8",
            maxshape=(None,),
            compression=LogWriter.compalg,
        )
        self.gpio_grp["time"].attrs["unit"] = f"system time in nano seconds"
        self.gpio_grp.create_dataset(
            "value",
            (0,),
            dtype="u1",
            maxshape=(None,),
            compression=LogWriter.compalg,
        )

        return self

    def __exit__(self, *exc):
        logger.info("flushing and closing hdf5 file")
        self._h5file.flush()
        self._h5file.close()

    def write_buffer(self, buffer: DataBuffer):
        """Writes data from buffer to file.
        
        Args:
            buffer (DataBuffer): Buffer containing IV data
        """

        # First, we have to resize the corresponding datasets
        current_length = self.data_grp["time"].shape[0]
        new_length = current_length + len(buffer)

        self.data_grp["time"].resize((new_length,))
        self.data_grp["time"][current_length:] = (
            buffer.timestamp_ns
            + self.sampling_interval * np.arange(len(buffer))
        )

        for variable in ["voltage", "current"]:
            self.data_grp[variable].resize((new_length,))
            self.data_grp[variable][current_length:] = getattr(buffer, variable)

        if len(buffer.gpio_edges) > 0:
            gpio_current_length = self.gpio_grp["time"].shape[0]
            gpio_new_length = gpio_current_length + len(buffer.gpio_edges)

            self.gpio_grp["time"].resize((gpio_new_length,))
            self.gpio_grp["value"].resize((gpio_new_length,))
            self.gpio_grp["time"][
                gpio_current_length:
            ] = buffer.gpio_edges.timestamps_ns
            self.gpio_grp["value"][
                gpio_current_length:
            ] = buffer.gpio_edges.values

    def write_exception(self, exception: ExceptionRecord):
        """Writes an exception to the hdf5 file.

        Args:
            exception (ExceptionRecord): The exception to be logged
        """
        current_length = self.log_grp["time"].shape[0]
        self.log_grp["time"].resize((current_length + 1,))
        self.log_grp["time"][current_length] = exception.timestamp
        self.log_grp["value"].resize((current_length + 1,))
        self.log_grp["value"][current_length] = exception.value
        self.log_grp["message"].resize((current_length + 1,))
        self.log_grp["message"][current_length] = exception.message

    def __setitem__(self, key, item):
        """Offer a convenient interface to store any relevant key-value data"""
        return self._h5file.attrs.__setitem__(key, item)


class LogReader(object):
    """ Sequentially Reads data from HDF5 file.

    Args:
        store_path (Path): Path of hdf5 file containing IV data
        samples_per_buffer (int): Number of IV samples per buffer
    """

    def __init__(self, store_path: Path, samples_per_buffer: int = 10_000):
        self.store_path = store_path
        self.samples_per_buffer = samples_per_buffer

    def __enter__(self):
        self._h5file = h5py.File(self.store_path, "r")
        self.ds_voltage = self._h5file["data"]["voltage"]
        self.ds_current = self._h5file["data"]["current"]
        return self

    def __exit__(self, *exc):
        self._h5file.close()

    def read_buffers(self, start: int = 0, end: int = None):
        """Reads the specified range of buffers from the hdf5 file.

        Args:
            start (int): Index of first buffer to be read
            end (int): Index of last buffer to be read
        
        Yields:
            Buffers between start and end
        """
        if end is None:
            end = int(
                self._h5file["data"]["time"].shape[0] / self.samples_per_buffer
            )
        logger.debug(f"Reading blocks from { start } to { end } from log")

        for i in range(start, end):
            ts_start = time.time()
            idx_start = i * self.samples_per_buffer
            idx_end = (i + 1) * self.samples_per_buffer
            db = DataBuffer(
                voltage=self.ds_voltage[idx_start:idx_end],
                current=self.ds_current[idx_start:idx_end],
            )
            logger.debug(
                (
                    f"Reading datablock with {self.samples_per_buffer} samples "
                    f"from netcdf took { time.time()-ts_start }"
                )
            )
            yield db

    def get_calibration_data(self):
        """Reads calibration data from hdf5 file.

        Returns:
            Calibration data as CalibrationData object
        """
        nested_dict = lambda: defaultdict(nested_dict)
        calib = nested_dict()
        for var, attr in product(["voltage", "current"], ["gain", "offset"]):

            calib["harvesting"][var][attr] = self._h5file["data"][var].attrs[
                attr
            ]
        return CalibrationData(calib)
