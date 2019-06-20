import os
import struct
import logging
import yaml
from pathlib import Path

from .calibration import CalibrationData

logger = logging.getLogger(__name__)

eeprom_format = {
    "header": {"offset": 0, "size": 4, "type": "binary"},
    "eeprom_revision": {"offset": 4, "size": 2, "type": "ascii"},
    "board_name": {"offset": 6, "size": 32, "type": "str"},
    "version": {"offset": 38, "size": 4, "type": "ascii"},
    "manufacturer": {"offset": 42, "size": 16, "type": "str"},
    "part_number": {"offset": 58, "size": 16, "type": "str"},
    "serial_number": {"offset": 76, "size": 12, "type": "ascii"},
}

calibration_data_format = {"offset": 512, "size": 96, "type": "binary"}


class CapeData(object):
    def __init__(self, data):
        self.data = data

    @classmethod
    def from_values(cls, serial_number, version="00A0"):

        data = {
            "header": b"\xAA\x55\x33\xEE",
            "eeprom_revision": "A1",
            "board_name": "BeagleBone SHEPHERD Cape",
            "version": version,
            "manufacturer": "NES TU DRESDEN",
            "part_number": "BB-SHPRD",
            "serial_number": serial_number,
        }
        return cls(data)

    @classmethod
    def from_yaml(cls, filename: Path):

        data = {"header": b"\xAA\x55\x33\xEE"}
        with open(filename, "r") as stream:
            yaml_dict = yaml.load(stream)

        data.update(yaml_dict)
        for key in eeprom_format.keys():
            if key not in data.keys():
                raise KeyError(f"Missing { key } from yaml file")

        return cls(data)

    def __getitem__(self, key):
        return self.data[key]

    def __repr__(self):
        print_dict = dict()
        for key in self.keys():
            if eeprom_format[key]["type"] in ["ascii", "str"]:
                print_dict[key] = self[key]
        return yaml.dump(print_dict, default_flow_style=False)

    def keys(self):
        return self.data.keys()

    def items(self):
        for key in self.keys():
            yield key, self[key]


class EEPROM(object):
    def __init__(self, bus_num=2, address=0x54):
        self.dev_path = f"/sys/bus/i2c/devices/{ bus_num }" f"-{address:04X}/eeprom"

    def __enter__(self):
        self.fd = os.open(self.dev_path, os.O_RDWR | os.O_SYNC)
        return self

    def __exit__(self, *args):
        os.close(self.fd)

    def _read(self, address, n_bytes):
        os.lseek(self.fd, address, 0)
        return os.read(self.fd, n_bytes)

    def _write(self, address, buffer):
        os.lseek(self.fd, address, 0)
        try:
            ret = os.write(self.fd, buffer)
        except TimeoutError:
            logger.error("Timeout writing to EEPROM. Is write protection disabled?")
            raise

    def __getitem__(self, key):
        if key not in eeprom_format.keys():
            raise KeyError(f"{ key } is not a valid EEPROM parameter")
        raw_data = self._read(eeprom_format[key]["offset"], eeprom_format[key]["size"])
        if eeprom_format[key]["type"] == "ascii":
            return raw_data.decode("utf-8")
        if eeprom_format[key]["type"] == "str":
            str_data = raw_data.split(b"\x00")
            return str_data[0].decode("utf-8")
        else:
            return raw_data

    def __setitem__(self, key, value):
        if key not in eeprom_format.keys():
            raise KeyError(f"{ key } is not a valid EEPROM parameter")
        if eeprom_format[key]["type"] == "ascii":
            if len(value) != eeprom_format[key]["size"]:
                raise ValueError(
                    (
                        f"Value { value } has wrong size. "
                        f"Required size is { eeprom_format[key]['size'] }"
                    )
                )
            self._write(eeprom_format[key]["offset"], value.encode("utf-8"))
        elif eeprom_format[key]["type"] == "str":
            if len(value) < eeprom_format[key]["size"]:
                value += "\0"
            elif len(value) > eeprom_format[key]["size"]:
                raise ValueError(
                    (
                        f"Value { value } is longer than maximum "
                        f"size { eeprom_format[key]['size'] }"
                    )
                )
            self._write(eeprom_format[key]["offset"], value.encode("utf-8"))
        else:
            self._write(eeprom_format[key]["offset"], value)

    def write_cape_data(self, cape_data: CapeData):
        for key, value in cape_data.items():
            self[key] = value

    def read_cape_data(self):
        data = dict()
        for key in eeprom_format.keys():
            data[key] = self[key]
        return CapeData(data)

    def write_calibration(self, calibration_data: CalibrationData):
        data = self._write(
            calibration_data_format["offset"], calibration_data.to_bytestr()
        )

    def read_calibration(self):
        data = self._read(
            calibration_data_format["offset"], calibration_data_format["size"]
        )
        return CalibrationData.from_bytestr(data)
