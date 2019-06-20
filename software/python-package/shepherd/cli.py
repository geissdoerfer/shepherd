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
import click_config_file
from periphery import GPIO
from bokeh.server.server import Server
from bokeh.application import Application
from bokeh.application.handlers.function import FunctionHandler
from shepherd import LogWriter
from shepherd import LogReader
from shepherd import Recorder
from shepherd import Emulator
from shepherd import CalibrationData
from shepherd import EEPROM
from shepherd import CapeData
from .shepherd_io import ShepherdRawIO
from .shepherd_io import ShepherdIOException
from .shepherd_io import MSG_ERR_INCOMPLETE
from .shepherd_io import ID_ERR_TIMEOUT
from .shepherd_io import gpio_pin_nums
from .datalog import ExceptionRecord
from .gui import make_document

consoleHandler = logging.StreamHandler()
logger = logging.getLogger("shepherd")
logger.addHandler(consoleHandler)


def unique_path(path, name_base):
    counter = 0
    directory = pathlib.Path(path)
    while True:
        path = directory / f"{ name_base }_{counter:04d}.h5"
        if not path.exists():
            return path
        counter += 1


def yamlprovider(file_path, cmd_name):
    logger.info(f"reading config from {file_path}")
    with open(file_path, "r") as config_data:
        full_config = yaml.safe_load(config_data)
    return full_config


def run_record(
    filepath, mode, length, force, defaultcalib, voltage, load, init_charge, start_time
):
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"

    gpio_led = GPIO(81, "out")

    if pathlib.Path(filepath).is_dir():
        rec_file = unique_path(filepath, "rec")
    else:
        rec_file = pathlib.Path(filepath).resolve()
        if not force and rec_file.exists():
            rec_file = unique_path(rec_file.parent, rec_file.stem)
            logger.warning(
                (
                    f"File {filepath} already exists.. "
                    f"storing under {str(rec_file)} instead"
                )
            )

    if defaultcalib:
        calib = CalibrationData.from_default()
    else:
        with EEPROM() as eeprom:
            calib = eeprom.read_calibration()

    recorder = Recorder(
        mode=mode, load=load, harvesting_voltage=voltage, init_charge=init_charge
    )
    log_writer = LogWriter(
        store_name=rec_file, calibration_data=calib, mode=mode, force=force
    )
    with ExitStack() as stack:

        stack.enter_context(recorder)
        stack.enter_context(log_writer)

        res = invoke.run("uptime", hide=True, warn=True)
        log_writer["uptime"] = res.stdout
        res = invoke.run("hostname", hide=True, warn=True)
        log_writer["hostname"] = res.stdout

        led_status = True

        recorder.start_sampling(start_time)
        if start_time is None:
            recorder.wait_for_start(15)
        else:
            logger.info(f"waiting {start_time - time.time():.2f}s until start")
            recorder.wait_for_start(start_time - time.time() + 15)
        logger.info("shepherd started!")

        def exit_gracefully(signum, frame):
            gpio_led.write(False)
            stack.close()
            sys.exit(0)

        signal.signal(signal.SIGTERM, exit_gracefully)
        signal.signal(signal.SIGINT, exit_gracefully)

        if length is None:
            ts_end = sys.float_info.max
        else:
            ts_end = time.time() + length

        while time.time() < ts_end:
            gpio_led.write(led_status)
            led_status = not led_status
            try:
                idx, buf = recorder.get_buffer()
            except ShepherdIOException as e:
                logger.error(f"ShepherdIOException(ID={e.id}, val={e.value}): {str(e)}")
                erec = ExceptionRecord(int(time.time() * 1e9), str(e), e.value)
                log_writer.write_exception(erec)

                if e.id == MSG_ERR_INCOMPLETE:
                    pass
                elif e.id == ID_ERR_TIMEOUT:
                    recorder.__exit__()
                    time.sleep(1)
                    recorder.__enter__()
                    recorder.start_sampling()
                    recorder.wait_for_start(15)
                    idx, buf = recorder.get_buffer(timeout=1)
                else:
                    raise

            log_writer.write_data(buf)
            recorder.put_buffer(idx)


def run_emulate(
    harvestingfile,
    loadfilepath,
    length,
    force,
    defaultcalib,
    load,
    init_charge,
    start_time,
):
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"

    gpio_led = GPIO(81, "out")
    led_status = False

    if defaultcalib:
        calib = CalibrationData.from_default()
    else:
        with EEPROM() as eeprom:
            calib = eeprom.read_calibration()

    if loadfilepath is not None:
        if pathlib.Path(loadfilepath).is_dir():
            load_file = unique_path(loadfilepath, "load")
        else:
            load_file = pathlib.Path(loadfilepath).resolve()
            if not force and load_file.exists():
                load_file = unique_path(load_file.parent, load_file.stem)
                logger.warning(
                    (
                        f"File {loadfilepath} already exists.. "
                        f"storing under {str(load_file)} instead"
                    )
                )

        log_writer = LogWriter(
            force=force, store_name=load_file, mode="load", calibration_data=calib
        )

    log_reader = LogReader(harvestingfile, 10000)

    with ExitStack() as stack:
        if loadfilepath is not None:
            stack.enter_context(log_writer)
        stack.enter_context(log_reader)

        emu = Emulator(
            calibration_recording=log_reader.get_calibration_data(),
            calibration_emulation=calib,
            init_charge=init_charge,
            load=load,
        )
        stack.enter_context(emu)

        # Preload emulator with some data
        for idx, buffer in enumerate(log_reader.read_blocks(end=10)):
            emu.put_buffer(idx, buffer)

        emu.start_sampling(start_time)
        if start_time is None:
            emu.wait_for_start(15)
        else:
            logger.info(f"waiting {start_time - time.time():.2f}s until start")
            emu.wait_for_start(start_time - time.time() + 15)

        logger.info("shepherd started!")

        def exit_gracefully(signum, frame):
            gpio_led.write(False)
            stack.close()
            sys.exit(0)

        signal.signal(signal.SIGTERM, exit_gracefully)
        signal.signal(signal.SIGINT, exit_gracefully)

        if length is None:
            ts_end = sys.float_info.max
        else:
            ts_end = time.time() + length

        for hrvst_buf in log_reader.read_blocks(start=10):
            try:
                idx, load_buf = emu.get_buffer(timeout=1)
            except ShepherdIOException as e:
                logger.error(
                    f"ShepherdIOException(ID={e.id}, val={e.value}): {str(e)}")
                erec = ExceptionRecord(int(time.time() * 1e9), str(e), e.value)
                if loadfilepath is not None:
                    log_writer.write_exception(erec)

                if e.id == MSG_ERR_INCOMPLETE:
                    pass
                else:
                    raise
            if loadfilepath is not None:
                log_writer.write_data(load_buf)

            emu.put_buffer(idx, hrvst_buf)

            if time.time() > ts_end:
                break
            gpio_led.write(led_status)
            led_status = not led_status
        
        if loadfilepath is not None:
            # Read all remaining buffers from PRU
            while(True):
                try:
                    idx, load_buf = emu.get_buffer(timeout=1)
                    log_writer.write_data(load_buf)
                except ShepherdIOException as e:
                    break



@click.group(context_settings=dict(help_option_names=["-h", "--help"], obj={}))
@click.option("-v", "--verbose", count=True, default=2)
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


@cli.command()
@click.option("--on/--off", default=True)
def targetpower(on):
    pin = GPIO(gpio_pin_nums["en_v_3v3"], "out")
    pin.write(not on)

    for pin_name in ["en_v_fix", "en_v_anlg", "en_lvl_cnv", "load"]:
        pin = GPIO(gpio_pin_nums[pin_name], "out")
        pin.write(on)


@cli.command()
@click.option("--command", default="record")
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

    if command in ["record", "emulate"]:
        fnc = getattr(sys.modules[__name__], f"run_{ command }")
        fnc(**parameters)
    else:
        raise click.BadParameter(f"command {command} not supported")


@cli.command(short_help="Record data")
@click.option("--filepath", type=click.Path(), default="/var/shepherd/recordings")
@click.option("--length", "-l", type=float)
@click.option("--force", "-f", is_flag=True)
@click.option(
    "--mode", type=click.Choice(["harvesting", "load", "both"]), default="harvesting"
)
@click.option("--defaultcalib", "-d", is_flag=True)
@click.option("--voltage", type=float)
@click.option("--load", type=click.Choice(["artificial", "node"]), default="artificial")
@click.option("--init-charge", is_flag=True)
@click.option("--start-time", type=int)
def record(
    filepath, mode, length, force, defaultcalib, voltage, load, init_charge, start_time
):
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"
    run_record(
        filepath,
        mode,
        length,
        force,
        defaultcalib,
        voltage,
        load,
        init_charge,
        start_time,
    )


@cli.command(short_help="Emulate data")
@click.argument("harvestingfile", type=click.Path(exists=True))
@click.option("--loadfilepath", "-o", type=click.Path())
@click.option("--length", "-l", type=float)
@click.option("--force", "-f", is_flag=True)
@click.option("--defaultcalib", "-d", is_flag=True)
@click.option("--load", type=click.Choice(["artificial", "node"]), default="node")
@click.option("--init-charge", is_flag=True)
@click.option("--start-time", type=int)
def emulate(
    harvestingfile,
    loadfilepath,
    length,
    force,
    defaultcalib,
    init_charge,
    load,
    start_time,
):
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"
    run_emulate(
        harvestingfile,
        loadfilepath,
        length,
        force,
        defaultcalib,
        load,
        init_charge,
        start_time,
    )


@cli.group(context_settings=dict(help_option_names=["-h", "--help"], obj={}))
def eeprom():
    "Shepherd: Synchronized Energy Harvesting Emulator and Recorder"
    pass


@eeprom.command(short_help="Write data to EEPROM")
@click.option("--infofile", "-i", type=click.Path(exists=True))
@click.option("--version", "-v", type=str)
@click.option("--serial_number", "-s", type=str)
@click.option("--calibfile", "-c", type=click.Path(exists=True))
@click.option("--defaultcalib", "-d", is_flag=True)
def write(infofile, version, serial_number, calibfile, defaultcalib):
    if infofile is not None:
        if serial_number is not None or version is not None:
            raise click.UsageError(
                ("--infofile and --version/--serial_number" " are mutually exclusive")
            )
        cape_data = CapeData.from_yaml(infofile)
        with EEPROM() as eeprom:
            eeprom.write_cape_data(cape_data)
    elif serial_number is not None or version is not None:
        if version is None or serial_number is None:
            raise click.UsageError(("--version and --serial_number are required"))
        cape_data = CapeData.from_values(serial_number, version)
        with EEPROM() as eeprom:
            cape_data = eeprom.write_cape_data(cape_data)

    if calibfile is not None:
        if defaultcalib:
            raise click.UsageError(
                "--defaultcalib and --calibfile are mutually exclusive"
            )
        calib = CalibrationData.from_yaml(calibfile)
        with EEPROM() as eeprom:
            cape_data = eeprom.write_calibration(calib)
    if defaultcalib:
        calib = CalibrationData.from_default()

        with EEPROM() as eeprom:
            eeprom.write_calibration(calib)


@eeprom.command(short_help="Read data from EEPROM")
@click.option("--infofile", "-i", type=click.Path())
@click.option("--calibfile", "-c", type=click.Path())
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


@eeprom.command(short_help="Convert calibration measurements to calibration data")
@click.argument("filename", type=click.Path(exists=True))
@click.option("--output", "-o", type=click.Path())
def make(filename, output):
    cd = CalibrationData.from_measurements(filename)
    if output is None:
        print(repr(cd))
    else:
        with open(output, "w") as f:
            f.write(repr(cd))


@cli.command(short_help="Show bokeh gui")
@click.option("--port", "-p", type=int, default=5006)
@click.option("--websocket-origin", "-o", type=str, default="*")
def gui(port, websocket_origin):

    apps = {"/": Application(FunctionHandler(make_document))}

    port = 5006
    websocket_origin = "*"

    server = Server(apps, port=port, allow_websocket_origin=[websocket_origin])

    def stop_server(sig, frame):
        server.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, stop_server)
    signal.signal(signal.SIGTERM, stop_server)

    server.run_until_shutdown()


@cli.command(short_help="Start zerorpc server")
@click.option("--port", "-p", type=int, default=4242)
def rpc(port):

    shepherd_io = ShepherdRawIO()
    shepherd_io.__enter__()

    server = zerorpc.Server(shepherd_io)
    server.bind(f"tcp://0.0.0.0:{ port }")

    def stop_server():
        server.stop()
        shepherd_io.__exit__()
        sys.exit(0)

    gevent.signal(signal.SIGTERM, stop_server)
    gevent.signal(signal.SIGINT, stop_server)

    server.run()


if __name__ == "__main__":
    cli()
