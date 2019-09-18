# -*- coding: utf-8 -*-

"""
shepherd.calibration
~~~~~
Provides CalibrationData class, defining the format of the SHEPHERD calibration
data


:copyright: (c) 2019 by Kai Geissdoerfer.
:license: MIT, see LICENSE for more details.
"""

import yaml
import struct
from scipy import stats
import numpy as np
from pathlib import Path

from . import calibration_default


class CalibrationData(object):
    """Represents SHEPHERD calibration data.

    Defines the format of calibration data and provides convenient functions
    to read and write calibration data.

    Args:
        calib_dict (dict): Dictionary containing calibration data.
    """

    def __init__(self, calib_dict: dict):
        self._data = calib_dict

    def __getitem__(self, key: str):
        return self._data[key]

    def __repr__(self):
        return yaml.dump(self._data, default_flow_style=False)

    @classmethod
    def from_bytestr(cls, bytestr: str):
        """Instantiates calibration data based on byte string.

        This is mainly used to deserialize data read from an EEPROM memory.

        Args:
            bytestr (str): Byte string containing calibration data.
        
        Returns:
            CalibrationData object with extracted calibration data.
        """
        vals = struct.unpack(">dddddddddddd", bytestr)
        calib_dict = dict()
        counter = 0
        for component in ["harvesting", "load", "emulation"]:
            calib_dict[component] = dict()
            for channel in ["voltage", "current"]:
                calib_dict[component][channel] = dict()
                for parameter in ["gain", "offset"]:
                    val = float(vals[counter])
                    if np.isnan(val):
                        raise ValueError(
                            f"{ component } { channel } { parameter } not a valid number"
                        )
                    calib_dict[component][channel][parameter] = val
                    counter += 1
        return cls(calib_dict)

    @classmethod
    def from_default(cls):
        """Instantiates calibration data from default hardware values.

        Returns:
            CalibrationData object with default calibration values.
        """
        calib_dict = dict()
        for component in ["harvesting", "load"]:
            calib_dict[component] = dict()
            for channel in ["voltage", "current"]:
                calib_dict[component][channel] = dict()
                offset = getattr(calibration_default, f"{ channel }_to_adc")(0)
                gain = (
                    getattr(calibration_default, f"{ channel }_to_adc")(1.0)
                    - offset
                )
                calib_dict[component][channel]["offset"] = -float(
                    offset
                ) / float(gain)
                calib_dict[component][channel]["gain"] = 1.0 / float(gain)

        calib_dict["emulation"] = dict()
        for channel in ["voltage", "current"]:
            calib_dict["emulation"][channel] = dict()
            offset = getattr(calibration_default, f"dac_to_{ channel }")(0)
            gain = (
                getattr(calibration_default, f"dac_to_{ channel }")(1.0)
                - offset
            )
            calib_dict["emulation"][channel]["offset"] = -float(
                offset
            ) / float(gain)
            calib_dict["emulation"][channel]["gain"] = 1.0 / float(gain)

        return cls(calib_dict)

    @classmethod
    def from_yaml(cls, filename: Path):
        """Instantiates calibration data from YAML file.

        Args:
            filename (Path): Path to YAML formatted file containing calibration
                values.
        
        Returns:
            CalibrationData object with extracted calibration data.
        """
        with open(filename, "r") as stream:
            in_data = yaml.safe_load(stream)

        return cls(in_data["calibration"])

    @classmethod
    def from_measurements(cls, filename: Path):
        """Instantiates calibration data from calibration measurements.

        Args:
            filename (Path): Path to YAML formatted file containing calibration
                measurement values.
        
        Returns:
            CalibrationData object with extracted calibration data.
        """
        with open(filename, "r") as stream:
            calib_data = yaml.safe_load(stream)

        calib_dict = dict()

        for component in ["harvesting", "load", "emulation"]:
            calib_dict[component] = dict()
            for channel in ["voltage", "current"]:
                calib_dict[component][channel] = dict()
                sample_points = calib_data["measurements"][component][channel]
                x = np.empty(len(sample_points))
                y = np.empty(len(sample_points))
                for i, point in enumerate(sample_points):
                    x[i] = point["measured"]
                    y[i] = point["reference"]
                slope, intercept, _, _, _ = stats.linregress(x, y)
                calib_dict[component][channel]["gain"] = float(slope)
                calib_dict[component][channel]["offset"] = float(intercept)

        return cls(calib_dict)

    def to_bytestr(self):
        """Serializes calibration data to byte string.

        Used to prepare data for writing it to EEPROM.

        Returns:
            Byte string representation of calibration values.
        """
        flattened = list()
        for component in ["harvesting", "load", "emulation"]:
            for channel in ["voltage", "current"]:
                for parameter in ["gain", "offset"]:
                    flattened.append(self._data[component][channel][parameter])

        return struct.pack(">dddddddddddd", *flattened)
