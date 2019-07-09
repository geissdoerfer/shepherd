import click
import re
import time
from fabric import Group
import numpy as np
from io import StringIO
from pathlib import Path
import telnetlib
import yaml
import logging

consoleHandler = logging.StreamHandler()
logger = logging.getLogger("shepherd-herd")
logger.addHandler(consoleHandler)


@click.group()
@click.argument("hosts", type=str)
@click.option("--user", "-u", type=str)
@click.option("--key-filename", "-k", type=click.Path(exists=True))
@click.option("-v", "--verbose", count=True, default=2)
@click.pass_context
def cli(ctx, hosts, verbose, user, key_filename):
    if Path(hosts).exists():
        with open(hosts, "r") as f:
            hostlist = [l.rstrip() for l in f.readlines()]
    else:
        hostlist = hosts.split(",")

    if verbose == 0:
        logger.setLevel(logging.ERROR)
    elif verbose == 1:
        logger.setLevel(logging.WARNING)
    elif verbose == 2:
        logger.setLevel(logging.INFO)
    elif verbose > 2:
        logger.setLevel(logging.DEBUG)

    ctx.obj["verbose"] = verbose

    connect_kwargs = dict()
    if key_filename is not None:
        connect_kwargs["key_filename"] = key_filename

    ctx.obj["fab group"] = Group(
        *hostlist, user=user, connect_kwargs=connect_kwargs
    )


@cli.command()
@click.pass_context
def poweroff(ctx):
    for cnx in ctx.obj["fab group"]:
        logger.info(f"powering off {cnx.host}")
        cnx.sudo("poweroff", hide=True, warn=True)


@cli.command()
@click.pass_context
@click.argument("cmd", type=str)
@click.option("--sudo", "-s", is_flag=True)
def run(ctx, cmd, sudo):
    for cnx in ctx.obj["fab group"]:
        if sudo:
            cnx.sudo(cmd, warn=True)
        else:
            cnx.run(cmd, warn=True)


@cli.group()
@click.option("--port", "-p", type=int, default=4444)
@click.pass_context
def target(ctx, port):
    ctx.obj["openocd_telnet_port"] = port


def start_openocd(cnx, timeout=30):
    cnx.sudo("systemctl start openocd", hide=True, warn=True)
    ts_end = time.time() + timeout
    while True:
        openocd_status = cnx.sudo(
            "systemctl status openocd", hide=True, warn=True
        )
        if openocd_status.exited == 0:
            break
        if time.time() > ts_end:
            raise TimeoutError(
                f"Timed out waiting for openocd on host {cnx.host}"
            )
        else:
            logger.debug(f"waiting for openocd on {cnx.host}")
            time.sleep(1)


@target.command()
@click.argument("image", type=click.Path(exists=True))
@click.pass_context
def flash(ctx, image):
    for cnx in ctx.obj["fab group"]:
        cnx.sudo("shepherd targetpower --on", hide=True, warn=True)
        openocd_status = cnx.sudo(
            "systemctl status openocd", hide=True, warn=True
        )
        if openocd_status.exited != 0:
            logger.debug(f"starting openocd on {cnx.host}")
            start_openocd(cnx)

    for cnx in ctx.obj["fab group"]:
        cnx.put(image, "/tmp/target_image.bin")

        with telnetlib.Telnet(cnx.host, ctx.obj["openocd_telnet_port"]) as tn:
            logger.debug(f"connected to openocd on {cnx.host}")
            tn.write(b"program /tmp/target_image.bin verify reset\n")
            res = tn.read_until(b"Verified OK", timeout=5)
            if b"Verified OK" in res:
                logger.info(f"flashed image on {cnx.host} successfully")
            else:
                logger.error(f"failed flashing image on {cnx.host}")


@target.command()
@click.pass_context
def halt(ctx):
    for cnx in ctx.obj["fab group"]:
        cnx.sudo("shepherd targetpower --on", hide=True, warn=True)
        openocd_status = cnx.sudo(
            "systemctl status openocd", hide=True, warn=True
        )
        if openocd_status.exited != 0:
            logger.debug(f"starting openocd on {cnx.host}")
            start_openocd(cnx)

    for cnx in ctx.obj["fab group"]:

        with telnetlib.Telnet(cnx.host, ctx.obj["openocd_telnet_port"]) as tn:
            logger.debug(f"connected to openocd on {cnx.host}")
            tn.write(b"halt\n")
            logger.info(f"target halted on {cnx.host}")


@target.command()
@click.pass_context
def reset(ctx):
    for cnx in ctx.obj["fab group"]:
        cnx.sudo("shepherd targetpower --on", hide=True, warn=True)
        openocd_status = cnx.sudo(
            "systemctl status openocd", hide=True, warn=True
        )
        if openocd_status.exited != 0:
            logger.debug(f"starting openocd on {cnx.host}")
            start_openocd(cnx)

    for cnx in ctx.obj["fab group"]:

        with telnetlib.Telnet(cnx.host, ctx.obj["openocd_telnet_port"]) as tn:
            logger.debug(f"connected to openocd on {cnx.host}")
            tn.write(b"reset\n")
            logger.info(f"target reset on {cnx.host}")


@cli.command()
@click.option(
    "--filepath", type=click.Path(), default="/var/shepherd/recordings"
)
@click.option("--length", "-l", type=int)
@click.option("--force", "-f", is_flag=True)
@click.option(
    "--mode",
    "-m",
    type=click.Choice(["harvesting", "load", "both"]),
    default="harvesting",
)
@click.option("--defaultcalib", "-d", is_flag=True)
@click.option("--voltage", type=float)
@click.option("--init-charge", is_flag=True)
@click.option(
    "--load", type=click.Choice(["artificial", "node"]), default="artificial"
)
@click.option("--start-delay", type=int)
@click.pass_context
def record(
    ctx,
    filepath,
    length,
    force,
    mode,
    defaultcalib,
    voltage,
    init_charge,
    load,
    start_delay,
):

    fp = Path(filepath)
    if not fp.is_absolute():
        fp = Path("/var/shepherd/recordings") / filepath

    if start_delay is not None:
        start_time = int(time.time()) + start_delay
    else:
        start_time = None

    parameter_dict = {
        "filepath": str(fp),
        "force": force,
        "length": length,
        "mode": mode,
        "defaultcalib": defaultcalib,
        "voltage": voltage,
        "init_charge": init_charge,
        "load": load,
        "start_time": start_time,
    }
    start_shepherd(
        ctx.obj["fab group"], "record", parameter_dict, ctx.obj["verbose"]
    )


def start_shepherd(group, command, parameters, verbose=0):
    config_dict = {
        "command": command,
        "verbose": verbose,
        "parameters": parameters,
    }
    config_yml = yaml.dump(config_dict, default_flow_style=False)

    for cnx in group:
        res = cnx.sudo("systemctl status shepherd", hide=True, warn=True)
        if res.exited != 3:
            raise Exception(f"shepherd not inactive on {cnx.host}")

        cnx.put(StringIO(config_yml), "/tmp/config.yml")
        cnx.sudo("mv /tmp/config.yml /etc/shepherd/config.yml")

        res = cnx.sudo("systemctl start shepherd", hide=True, warn=True)


@cli.command(short_help="Emulate data")
@click.argument("harvestingfile", type=click.Path())
@click.option("--loadfilepath", "-o", type=click.Path())
@click.option("--length", "-l", type=float)
@click.option("--force", "-f", is_flag=True)
@click.option("--defaultcalib", "-d", is_flag=True)
@click.option(
    "--load", type=click.Choice(["artificial", "node"]), default="node"
)
@click.option("--init-charge", is_flag=True)
@click.option("--start-delay", type=int)
@click.pass_context
def emulate(
    ctx,
    harvestingfile,
    loadfilepath,
    length,
    force,
    defaultcalib,
    load,
    init_charge,
    start_delay,
):
    fp = Path(harvestingfile)
    if not fp.is_absolute():
        fp = Path("/var/shepherd/recordings") / harvestingfile

    if start_delay is not None:
        start_time = int(time.time()) + start_delay
    else:
        start_time = None
    parameter_dict = {
        "harvestingfile": str(fp),
        "loadfilepath": loadfilepath,
        "force": force,
        "length": length,
        "defaultcalib": defaultcalib,
        "init_charge": init_charge,
        "start_time": start_time,
        "load": load,
    }
    start_shepherd(
        ctx.obj["fab group"], "emulate", parameter_dict, ctx.obj["verbose"]
    )


@cli.command()
@click.pass_context
def stop(ctx):
    for cnx in ctx.obj["fab group"]:
        cnx.sudo("systemctl stop shepherd", hide=True, warn=True)


@cli.command()
@click.argument("filename", type=click.Path())
@click.argument("outdir", type=click.Path(exists=True))
@click.option("--rename", "-r", is_flag=True)
@click.option("--delete", "-d", is_flag=True)
@click.option("--stop", "-s", is_flag=True)
@click.pass_context
def retrieve(ctx, filename, outdir, rename, delete, stop):
    if stop:
        for cnx in ctx.obj["fab group"]:

            logger.info(f"stopping shepherd service on {cnx.host}")
            res = cnx.sudo("systemctl stop shepherd", hide=True, warn=True)

    time_str = time.strftime("%Y_%m_%dT%H_%M_%S")
    ts_end = time.time() + 30
    for cnx in ctx.obj["fab group"]:
        while True:
            res = cnx.sudo("systemctl status shepherd", hide=True, warn=True)
            if res.exited == 3:
                break
            if not stop or time.time() > ts_end:
                raise Exception(f"shepherd not inactive on {cnx.host}")
            time.sleep(1)

        target_path = Path(outdir) / cnx.host
        if not target_path.exists():
            logger.info(f"creating local dir {target_path}")
            target_path.mkdir()

        if Path(filename).is_absolute():
            filepath = Path(filename)
        else:
            filepath = Path("/var/shepherd/recordings") / filename

        if rename:
            local_path = target_path / f"rec_{ time_str }.h5"
        else:
            local_path = target_path / filepath.name

        logger.info(
            (
                f"retrieving remote file {filepath} from "
                f"{cnx.host} to local {local_path}"
            )
        )
        cnx.get(filepath, local=local_path)
        if delete:
            logger.info(f"deleting {filepath} from remote {cnx.host}")
            cnx.sudo(f"rm {str(filepath)}", hide=True)


def main():
    return cli(obj={})
