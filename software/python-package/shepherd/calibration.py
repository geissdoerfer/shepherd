import yaml
import struct
from scipy import stats
import numpy as np
from pathlib import Path

from . import calibration_default


class CalibrationData(object):
    def __init__(self, calib_dict):
        self._data = calib_dict

    def __getitem__(self, key):
        return self._data[key]

    def __repr__(self):
        return yaml.dump(self._data, default_flow_style=False)

    @classmethod
    def from_bytestr(cls, bytestr: str):
        vals = struct.unpack(">dddddddddddd", bytestr)
        calib_dict = dict()
        counter = 0
        for component in ["harvesting", "load", "emulation"]:
            calib_dict[component] = dict()
            for channel in ["voltage", "current"]:
                calib_dict[component][channel] = dict()
                for parameter in ["gain", "offset"]:
                    calib_dict[component][channel][parameter] = float(vals[counter])
                    counter += 1
        return cls(calib_dict)

    @classmethod
    def from_default(cls):

        calib_dict = dict()
        for component in ["harvesting", "load"]:
            calib_dict[component] = dict()
            for channel in ["voltage", "current"]:
                calib_dict[component][channel] = dict()
                offset = getattr(calibration_default, f"{ channel }_to_adc")(0)
                gain = getattr(calibration_default, f"{ channel }_to_adc")(1.0) - offset
                calib_dict[component][channel]["offset"] = -float(offset) / float(gain)
                calib_dict[component][channel]["gain"] = 1.0 / float(gain)

        calib_dict["emulation"] = dict()
        for channel in ["voltage", "current"]:
            calib_dict["emulation"][channel] = dict()
            offset = getattr(calibration_default, f"dac_to_{ channel }")(0)
            gain = getattr(calibration_default, f"dac_to_{ channel }")(1.0) - offset
            calib_dict["emulation"][channel]["offset"] = -float(offset) / float(gain)
            calib_dict["emulation"][channel]["gain"] = 1.0 / float(gain)

        return cls(calib_dict)

    @classmethod
    def from_yaml(cls, filename: Path):
        with open(filename, "r") as stream:
            in_data = yaml.safe_load(stream)

        return cls(in_data["calibration"])

    @classmethod
    def from_measurements(cls, filename: Path):
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
        flattened = list()
        for component in ["harvesting", "load", "emulation"]:
            for channel in ["voltage", "current"]:
                for parameter in ["gain", "offset"]:
                    flattened.append(self._data[component][channel][parameter])

        return struct.pack(">dddddddddddd", *flattened)
