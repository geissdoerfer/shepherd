# -*- coding: utf-8 -*-

"""
shepherd.cli
~~~~~
Provides the CLI utility 'shepherd-sheep', exposing most of shepherd's
functionality to a command line user.


:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""
import click
import time
import pathlib
import logging
import sys
import signal
import zerorpc
import gevent
import yaml
from contextlib import ExitStack
import invoke
from pathlib import Path
import click_config_file
from periphery import GPIO

from shepherd import LogWriter
from shepherd import LogReader
from shepherd import record as run_record
from shepherd import emulate as run_emulate
from shepherd import CalibrationData
from shepherd import EEPROM
from shepherd import CapeData
from shepherd import ShepherdDebug
from shepherd.shepherd_io import gpio_pin_nums
from shepherd.const_reg import VariableLDO

consoleHandler = logging.StreamHandler()
logger = logging.getLogger("shepherd")
logger.addHandler(consoleHandler)


def yamlprovider(file_path, cmd_name):
    logger.info(f"reading config from {file_path}")
    with open(file_path, "r") as config_data:
        full_config = yaml.safe_load(config_data)
    return full_config


@click.group(context_settings=dict(help_option_names=["-h", "--help"], obj={}))
@click.option("-v", "--verbose", count=True, default=1)
@click.pass_context
def cli(ctx, verbose):
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"

    if verbose == 0:
        logger.setLevel(logging.ERROR)
    elif verbose == 1:
        logger.setLevel(logging.WARNING)
    elif verbose == 2:
        logger.setLevel(logging.INFO)
    elif verbose > 2:
        logger.setLevel(logging.DEBUG)


@cli.command(short_help="Turns sensor node power supply on or off")
@click.option("--on/--off", default=True)
@click.option("--voltage", type=float, help="Target supply voltage")
def targetpower(on, voltage):
    if not voltage:
        voltage = 3.0
    else:
        if not on:
            raise click.UsageError(
                "Can't set voltage, when LDO is switched off"
            )
    for pin_name in ["en_v_anlg", "en_lvl_cnv", "load"]:
        pin = GPIO(gpio_pin_nums[pin_name], "out")
        pin.write(on)

    with VariableLDO() as ldo:
        if on:
            ldo.set_voltage(voltage)
        ldo.set_output(on)


@cli.command(
    short_help="Runs a command with given parameters. Mainly for use with config file."
)
@click.option(
    "--command", default="record", type=click.Choice(["record", "emulate"])
)
@click.option("--parameters", default=dict())
@click.option("-v", "--verbose", count=True)
@click_config_file.configuration_option(provider=yamlprovider)
def run(command, parameters, verbose):

    if verbose is not None:
        if verbose == 0:
            logger.setLevel(logging.ERROR)
        elif verbose == 1:
            logger.setLevel(logging.WARNING)
        elif verbose == 2:
            logger.setLevel(logging.INFO)
        elif verbose > 2:
            logger.setLevel(logging.DEBUG)

    if command == "record":
        if "output" in parameters.keys():
            parameters["output"] = Path(parameters["output"])
        run_record(**parameters)
    elif command == "emulate":
        if "output" in parameters.keys():
            parameters["output"] = Path(parameters["output"])
        if "input" in parameters.keys():
            parameters["input"] = Path(parameters["input"])
        run_emulate(**parameters)
    else:
        raise click.BadParameter(f"command {command} not supported")


@cli.command(short_help="Record data")
@click.option(
    "--output",
    "-o",
    type=click.Path(),
    default="/var/shepherd/recordings",
    help="Dir or file path for resulting hdf5 file",
)
@click.option(
    "--mode",
    type=click.Choice(["harvesting", "load"]),
    default="harvesting",
    help="Record 'harvesting' or 'load' data",
)
@click.option(
    "--length", "-l", type=float, help="Duration of recording in seconds"
)
@click.option("--force", "-f", is_flag=True, help="Overwrite existing file")
@click.option("--no-calib", is_flag=True, help="Use default calibration values")
@click.option(
    "--voltage", type=float, help="Set fixed reference voltage for harvesting"
)
@click.option(
    "--load",
    type=click.Choice(["artificial", "node"]),
    default="artificial",
    help="Choose artificial or sensor node load",
)
@click.option(
    "--ldo-voltage",
    "-c",
    type=float,
    default=2.1,
    help="Set voltage of variable LDO regulator",
)
@click.option(
    "--start-time",
    "-s",
    type=float,
    help="Desired start time in unix epoch time",
)
@click.option(
    "--warn-only/--no-warn-only", default=True, help="Warn only on errors"
)
def record(
    output,
    mode,
    length,
    force,
    no_calib,
    voltage,
    load,
    ldo_voltage,
    start_time,
    warn_only,
):
    pl_store = Path(output)
    if pl_store.is_dir():
        pl_store = pl_store / "rec.h5"

    run_record(
        pl_store,
        mode,
        length,
        force,
        no_calib,
        voltage,
        load,
        ldo_voltage,
        start_time,
        warn_only,
    )


@cli.command(
    short_help="Emulate data, where INPUT is an hdf5 file containing harvesting data"
)
@click.argument("input", type=click.Path(exists=True))
@click.option(
    "--output",
    "-o",
    type=click.Path(),
    help="Dir or file path for storing the load consumption data",
)
@click.option(
    "--length", "-l", type=float, help="Duration of recording in seconds"
)
@click.option("--force", "-f", is_flag=True, help="Overwrite existing file")
@click.option("--no-calib", is_flag=True, help="Use default calibration values")
@click.option(
    "--load",
    type=click.Choice(["artificial", "node"]),
    default="node",
    help="Choose artificial or sensor node load",
)
@click.option(
    "--ldo-voltage",
    "-c",
    help="Pre-charge capacitor to this voltage before starting emulation",
    type=float,
    default=2.1,
)
@click.option(
    "--start-time", type=float, help="Desired start time in unix epoch time"
)
@click.option(
    "--warn-only/--no-warn-only", default=True, help="Warn only on errors"
)
def emulate(
    input,
    output,
    length,
    force,
    no_calib,
    load,
    ldo_voltage,
    start_time,
    warn_only,
):
    if output is None:
        pl_store = None
    else:
        pl_store = Path(output)
        if pl_store.is_dir():
            pl_store = pl_store / "rec.h5"
        else:
            pl_store = Path("/var/shepherd/recordings") / output

    run_emulate(
        input,
        pl_store,
        length,
        force,
        no_calib,
        load,
        ldo_voltage,
        start_time,
        warn_only,
    )


@cli.group(
    context_settings=dict(help_option_names=["-h", "--help"], obj={}),
    short_help="Read/Write data from EEPROM",
)
def eeprom():
    pass


@eeprom.command(short_help="Write data to EEPROM")
@click.option(
    "--infofile",
    "-i",
    type=click.Path(exists=True),
    help="YAML-formatted file with cape info",
)
@click.option(
    "--version",
    "-v",
    type=str,
    help="Cape version number, e.g. 00A0",
    default="00A0",
)
@click.option(
    "--serial_number",
    "-s",
    type=str,
    help="Cape serial number, e.g. 3219AAAA0001",
)
@click.option(
    "--calibfile",
    "-c",
    type=click.Path(exists=True),
    help="YAML-formatted file with calibration data",
)
@click.option("--no-calib", is_flag=True, help="Use default calibration data")
def write(infofile, version, serial_number, calibfile, no_calib):
    if infofile is not None:
        if serial_number is not None or version is not None:
            raise click.UsageError(
                (
                    "--infofile and --version/--serial_number"
                    " are mutually exclusive"
                )
            )
        cape_data = CapeData.from_yaml(infofile)
        with EEPROM() as eeprom:
            eeprom.write_cape_data(cape_data)
    elif serial_number is not None or version is not None:
        if version is None or serial_number is None:
            raise click.UsageError(
                ("--version and --serial_number are required")
            )
        cape_data = CapeData.from_values(serial_number, version)
        with EEPROM() as eeprom:
            eeprom.write_cape_data(cape_data)

    if calibfile is not None:
        if no_calib:
            raise click.UsageError(
                "--no-calib and --calibfile are mutually exclusive"
            )
        calib = CalibrationData.from_yaml(calibfile)
        with EEPROM() as eeprom:
            cape_data = eeprom.write_calibration(calib)
    if no_calib:
        calib = CalibrationData.from_default()

        with EEPROM() as eeprom:
            eeprom.write_calibration(calib)


@eeprom.command(short_help="Read cape info and calibration data from EEPROM")
@click.option(
    "--infofile",
    "-i",
    type=click.Path(),
    help="If provided, cape info data is dumped to this file",
)
@click.option(
    "--calibfile",
    "-c",
    type=click.Path(),
    help="If provided, calibration data is dumped to this file",
)
def read(infofile, calibfile):
    with EEPROM() as eeprom:
        cape_data = eeprom.read_cape_data()
        calib = eeprom.read_calibration()

    if infofile:
        with open(infofile, "w") as f:
            f.write(repr(cape_data))
    else:
        print(repr(cape_data))

    if calibfile:
        with open(calibfile, "w") as f:
            f.write(repr(calib))
    else:
        print(repr(calib))


@eeprom.command(
    short_help="Convert calibration measurements to calibration data, where FILENAME is YAML-formatted file containing calibration measurements"
)
@click.argument("filename", type=click.Path(exists=True))
@click.option(
    "--output",
    "-o",
    type=click.Path(),
    help="Path to resulting YAML-formatted calibration data file",
)
def make(filename, output):
    cd = CalibrationData.from_measurements(filename)
    if output is None:
        print(repr(cd))
    else:
        with open(output, "w") as f:
            f.write(repr(cd))


@cli.command(short_help="Start zerorpc server")
@click.option("--port", "-p", type=int, default=4242)
def rpc(port):

    shepherd_io = ShepherdDebug()
    shepherd_io.__enter__()
    logger.debug("Initialized shepherd debug interface")
    time.sleep(1)

    server = zerorpc.Server(shepherd_io)
    server.bind(f"tcp://0.0.0.0:{ port }")
    time.sleep(1)

    def stop_server():
        server.stop()
        shepherd_io.__exit__()
        sys.exit(0)

    gevent.signal(signal.SIGTERM, stop_server)
    gevent.signal(signal.SIGINT, stop_server)

    shepherd_io.start()
    logger.debug("Started shepherd debug interface")
    server.run()


if __name__ == "__main__":
    cli()
