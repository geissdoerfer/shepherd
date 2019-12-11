#!/usr/bin/env python3
import time
from keithley2600b import SMU
import click
import zerorpc
import sys
import yaml
import numpy as np
import tempfile
from scipy import stats
from fabric import Connection
import matplotlib.pyplot as plt
from sklearn.linear_model import LinearRegression

V_REF_DAC = 2.5
G_DAC_A = 1.0
G_DAC_V = 2.0

INSTR_HRVST = """
---------- Harvesting calibration -------------
Connect SMU channel B hi to P1-2 and lo to P1-1.
Connect a 90-120 Ohm resistor between JP2-2 and GND.
"""

INSTR_LOAD = """
------------- Load calibration ----------------
Connect SMU channel A hi to JP1-2 and lo to GND.
Connect a 91 Ohm resistor between P2-1 and P2-2.
"""

INSTR_EMU = """
---------------------- Emulation calibration -----------------------
Connect SMU channel B lo to JP2-2 and hi to GND via a 91 Ohm resistor.
Connect SMU channel A hi to JP2-1 and lo to GND.
Mount a storage capelet with a 100uF-220uF capacitor
Connect a jumper between JP1-1 and JP1-2
"""


def measurements_to_calibration(measurements):

    calib_dict = dict()

    for component in ["harvesting", "load", "emulation"]:
        calib_dict[component] = dict()
        for channel in ["voltage", "current"]:
            calib_dict[component][channel] = dict()
            sample_points = measurements[component][channel]
            x = np.empty(len(sample_points))
            y = np.empty(len(sample_points))
            for i, point in enumerate(sample_points):
                x[i] = point["measured"]
                y[i] = point["reference"]
            WLS = LinearRegression()
            WLS.fit(x.reshape(-1, 1), y.reshape(-1, 1), sample_weight=1.0 / x)
            intercept = WLS.intercept_
            slope = WLS.coef_[0]
            calib_dict[component][channel]["gain"] = float(slope)
            calib_dict[component][channel]["offset"] = float(intercept)
    return calib_dict


def measure_current(rpc_client, smu_channel, adc_channel):

    values = [0.00001, 0.0001, 0.001, 0.01, 0.025]
    rpc_client.dac_write("current", 0)
    rpc_client.dac_write("voltage", 0)

    smu_channel.configure_isource(range=0.05)
    results = list()
    for val in values:
        smu_channel.set_current(val, vlimit=3.0)
        smu_channel.set_output(True)
        time.sleep(0.25)

        meas = np.empty(10)
        for i in range(10):
            meas[i] = rpc_client.adc_read(adc_channel)
        meas_avg = float(np.mean(meas))
        results.append({"reference": val, "measured": meas_avg})
        print(f"ref: {val*1000:.4f}mA meas: {meas_avg}")

        smu_channel.set_output(False)

    return results


def measure_voltage(rpc_client, smu_channel, adc_channel):

    values = [0.1, 0.5, 1.0, 2.0, 2.8, 3.0]
    rpc_client.dac_write("current", 0)
    rpc_client.dac_write("voltage", 0)

    smu_channel.configure_vsource(range=4.0)
    results = list()
    for val in values:
        smu_channel.set_voltage(val, ilimit=0.05)
        smu_channel.set_output(True)
        time.sleep(0.25)
        meas = np.empty(10)
        for i in range(10):
            meas[i] = rpc_client.adc_read(adc_channel)

        meas_avg = float(np.mean(meas))
        results.append({"reference": val, "measured": meas_avg})
        print(f"ref: {val}V meas: {meas_avg}")

        smu_channel.set_output(False)
    return results


def emulate_current(rpc_client, smu_channel):

    values = [
        int(val)
        for val in [100, 500, 800, 1000, 2500, 5000, 2 ** 14, 2 ** 16 - 512]
    ]
    rpc_client.set_harvester(False)
    rpc_client.dac_write("voltage", 0)

    smu_channel.configure_vsource(range=0.001)
    smu_channel.set_voltage(0.0, ilimit=0.03)
    smu_channel.set_output(True)
    results = list()
    for i, val in enumerate(values):
        rpc_client.dac_write("current", val)

        time.sleep(0.25)

        meas = smu_channel.measure_current(range=0.03, nplc=2.0)
        # The current source is non-linear in the lowest region, because it is
        # designed to guarantee zero current output. Thus, a small DAC value
        # would theoretically lead to a negative current. Due to the output
        # stage of the source, this leads to zero output for inputs below
        # around 800. All values that fall into this region need to be
        # dismissed for calibration as they would distort the otherwise linear
        # relation
        if meas < 1e-6:
            continue

        results.append({"reference": val, "measured": meas})
        print(f"ref: {val} meas: {meas*1000:.4f}mA")

    smu_channel.set_output(False)
    return results


def emulate_voltage(rpc_client, smu_channel):

    voltages = np.linspace(0.3, 2.8, 10)

    values = [int(val) for val in (voltages / G_DAC_V / V_REF_DAC) * (2 ** 16)]

    rpc_client.dac_write("A", 0)
    rpc_client.set_mppt(False)
    rpc_client.set_load("artificial")
    rpc_client.set_ldo_voltage(3.0)

    smu_channel.configure_isource(range=0.001)
    smu_channel.set_current(0.0005, vlimit=5.0)
    smu_channel.set_output(True)

    results = list()
    for val in values:
        rpc_client.dac_write("V", val)

        time.sleep(0.5)

        meas = smu_channel.measure_voltage(range=3.0, nplc=1.0)

        results.append({"reference": val, "measured": meas})
        print(f"ref: {val} meas: {meas}V")

    smu_channel.set_output(False)
    return results


@click.group(context_settings=dict(help_option_names=["-h", "--help"], obj={}))
def cli():
    pass


@cli.command()
@click.argument("host", type=str)
@click.option("--user", "-u", type=str, default="joe")
@click.option("--outfile", "-o", type=click.Path())
@click.option("--smu-ip", type=str, default="192.168.1.108")
@click.option("--all", "all_", is_flag=True)
@click.option("--harvesting", is_flag=True)
@click.option("--load", is_flag=True)
@click.option("--emulation", is_flag=True)
def measure(host, user, outfile, smu_ip, all_, harvesting, load, emulation):

    if all_:
        if harvesting or load or emulation:
            raise click.UsageError("Either provide --all or individual flags")

        harvesting = True
        load = True
        emulation = True
    if not any([all_, harvesting, load, emulation]):
        harvesting = True
        load = True
        emulation = True

    rpc_client = zerorpc.Client(timeout=60, heartbeat=20)
    measurement_dict = dict()

    with SMU.ethernet_device(smu_ip) as smu, Connection(host, user=user) as cnx:
        res = cnx.sudo("systemctl restart shepherd-rpc", hide=True, warn=True)
        rpc_client.connect(f"tcp://{ host }:4242")

        if harvesting:

            click.echo(INSTR_HRVST)
            usr_conf = click.confirm("Everything setup?")

            if usr_conf:
                measurement_dict["harvesting"] = {
                    "voltage": list(),
                    "current": list(),
                }
                rpc_client.set_harvester(True)
                measurement_dict["harvesting"]["current"] = measure_current(
                    rpc_client, smu.B, "A_IN"
                )
                measurement_dict["harvesting"]["voltage"] = measure_voltage(
                    rpc_client, smu.B, "V_IN"
                )
                rpc_client.set_harvester(False)

        if load:
            click.echo(INSTR_LOAD)
            usr_conf = click.confirm("Everything setup?")

            if usr_conf:
                measurement_dict["load"] = {
                    "voltage": list(),
                    "current": list(),
                }
                rpc_client.set_load("node")
                measurement_dict["load"]["current"] = measure_current(
                    rpc_client, smu.A, "A_OUT"
                )
                measurement_dict["load"]["voltage"] = measure_voltage(
                    rpc_client, smu.A, "V_OUT"
                )

        if emulation:
            click.echo(INSTR_EMU)
            usr_conf = click.confirm("Everything setup?")
            if usr_conf:
                measurement_dict["emulation"] = {
                    "voltage": list(),
                    "current": list(),
                }
                measurement_dict["emulation"]["current"] = emulate_current(
                    rpc_client, smu.B
                )
                measurement_dict["emulation"]["voltage"] = emulate_voltage(
                    rpc_client, smu.A
                )

        out_dict = {"node": host, "measurements": measurement_dict}
        res = cnx.sudo("systemctl stop shepherd-rpc", hide=True, warn=True)
        res_repr = yaml.dump(out_dict, default_flow_style=False)
        if outfile is not None:
            with open(outfile, "w") as f:
                f.write(res_repr)
        else:
            print(res_repr)


@cli.command()
@click.argument("infile", type=click.Path(exists=True))
@click.option("--outfile", "-o", type=click.Path())
def convert(infile, outfile):
    with open(infile, "r") as stream:
        in_data = yaml.safe_load(stream)
    measurement_dict = in_data["measurements"]

    calib_dict = measurements_to_calibration(measurement_dict)

    out_dict = {"node": in_data["node"], "calibration": calib_dict}
    res_repr = yaml.dump(out_dict, default_flow_style=False)
    if outfile is not None:
        with open(outfile, "w") as f:
            f.write(res_repr)
    else:
        print(res_repr)


@cli.command()
@click.argument("host", type=str)
@click.option("--calibfile", "-c", type=click.Path(exists=True))
@click.option("--measurementfile", "-m", type=click.Path(exists=True))
@click.option("--version", "-v", type=str, default="00A0")
@click.option("--serial_number", "-s", type=str, required=True)
@click.option("--user", "-u", type=str, default="joe")
def write(host, calibfile, measurementfile, version, serial_number, user):

    if calibfile is None:
        if measurementfile is None:
            raise click.UsageError(
                "provide one of calibfile or measurementfile"
            )

        with open(measurementfile, "r") as stream:
            in_measurements = yaml.safe_load(stream)
        measurement_dict = in_measurements["measurements"]
        in_data = dict()
        in_data["calibration"] = measurements_to_calibration(measurement_dict)
        in_data["node"] = in_measurements["node"]
        res_repr = yaml.dump(in_data, default_flow_style=False)
        tmp_file = tempfile.NamedTemporaryFile()
        calibfile = tmp_file.name
        with open(calibfile, "w") as f:
            f.write(res_repr)

    else:
        if measurementfile is not None:
            raise click.UsageError(
                "provide only one of calibfile or measurementfile"
            )
        with open(calibfile, "r") as stream:
            in_data = yaml.safe_load(stream)

    if in_data["node"] != host:
        click.confirm(
            (
                f"Calibration data for '{ in_data['node'] }' doesn't match "
                f"host '{ host }'. Do you wish to proceed?"
            ),
            abort=True,
        )
    with Connection(host, user=user) as cnx:
        cnx.put(calibfile, "/tmp/calib.yml")
        usr_conf = click.confirm("Write-protection disabled?", abort=True)
        cnx.sudo(
            (
                f"shepherd-sheep eeprom write -v { version } -s {serial_number}"
                " -c /tmp/calib.yml"
            )
        )
        click.echo("----------EEPROM READ------------")
        cnx.sudo("shepherd-sheep eeprom read")
        click.echo("---------------------------------")


@cli.command()
@click.argument("host", type=str)
@click.option("--user", "-u", type=str, default="joe")
def read(host, user):

    with Connection(host, user=user) as cnx:
        click.echo("----------EEPROM READ------------")
        cnx.sudo("shepherd-sheep eeprom read")
        click.echo("---------------------------------")


if __name__ == "__main__":
    cli()
