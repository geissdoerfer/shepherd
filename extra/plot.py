import h5py
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
from pathlib import Path
import click
from scipy.signal import decimate


def ds_to_phys(dataset: h5py.Dataset):
    gain = dataset.attrs["gain"]
    offset = dataset.attrs["offset"]
    return dataset[:] * gain + offset


def extract_hdf(hdf_file: Path, ds_factor: int = 1):
    with h5py.File(hdf_file, "r") as hf:
        data = dict()
        times = hf["data"]["time"][:].astype(float) / 1e9

        for var in ["voltage", "current"]:
            sig_phys = ds_to_phys(hf["data"][var])
            if ds_factor > 1:
                sig_phys = decimate(sig_phys, ds_factor, ftype="fir")
            data[var] = sig_phys

        if ds_factor > 1:
            data["time"] = times[::ds_factor] - times[0]
        else:
            data["time"] = times - times[0]

    return data


@click.command(short_help="Plot shepherd data from DIR")
@click.option("--directory", "-d", type=click.Path(exists=True))
@click.option("--filename", "-f", type=click.Path(), default="rec.h5")
@click.option("--sampling-rate", "-s", type=int, default=1000)
@click.option("--limit", "-l", type=str)
def cli(directory, filename, sampling_rate, limit):

    ds_factor = int(100000 / sampling_rate)
    f, axes = plt.subplots(2, 1, sharex=True)
    f.suptitle(f"Voltage and current @ {sampling_rate}Hz")

    if directory is None:
        hdf_file = Path(filename)
        if not hdf_file.exists():
            raise click.FileError(str(hdf_file), hint="File not found")
        data = extract_hdf(hdf_file, ds_factor=ds_factor)
        axes[0].plot(data["time"], data["voltage"])
        axes[1].plot(data["time"], data["current"] * 1e6)
    else:
        data = dict()
        pl_dir = Path(directory)

        if limit:
            active_nodes = limit.split(",")
        else:
            active_nodes = [child.stem for child in list(pl_dir.iterdir())]

        for child in pl_dir.iterdir():
            if not child.stem in active_nodes:
                continue

            hdf_file = child / filename
            if not hdf_file.exists():
                raise click.FileError(str(hdf_file), hint="File not found")
            hostname = child.stem
            data[hostname] = extract_hdf(hdf_file, ds_factor=ds_factor)
            axes[0].plot(
                data[hostname]["time"],
                data[hostname]["voltage"],
                label=hostname,
            )
            axes[1].plot(
                data[hostname]["time"],
                data[hostname]["current"] * 1e6,
                label=hostname,
            )

    axes[0].set_ylabel("voltage [V]")
    axes[1].set_ylabel(r"current [$\mu$A]")
    axes[0].legend()
    axes[1].legend()
    axes[1].set_xlabel("time [s]")
    plt.show()


if __name__ == "__main__":
    cli()
