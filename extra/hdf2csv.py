import h5py
import click
import itertools as it
from pathlib import Path
from datetime import datetime
import numpy as np
from scipy import signal


def downsample_signal(
    dataset: h5py.Dataset, ds_factor: int, block_len: int = None
):
    # 8th order chebyshev filter for downsampling
    flt = signal.iirfilter(
        N=8,
        Wn=1 / ds_factor,
        btype="lowpass",
        output="sos",
        ftype="cheby1",
        rp=3,
    )

    if block_len is None:
        # Aim for 100 blocks
        n_blocks = 100
        block_len = int(len(dataset) / n_blocks)
    else:
        n_blocks = int(len(dataset) / block_len)

    # filter state
    z = np.zeros((flt.shape[0], 2))

    block_ds_len = int(block_len / ds_factor)
    sig_ds = np.empty((block_ds_len * n_blocks,))
    for i in range(n_blocks):
        y, z = signal.sosfilt(
            flt, dataset[i * block_len : (i + 1) * block_len], zi=z
        )

        sig_ds[i * block_ds_len : (i + 1) * block_ds_len] = y[::ds_factor]
        print(f"block {i+1}/{n_blocks} done")
    return sig_ds


def downsample_time(time, ds_factor: int, block_len: int = 1000000):
    n_blocks = int(len(time) / block_len)

    block_ds_len = int(block_len / ds_factor)
    time_ds = np.empty((block_ds_len * n_blocks,))
    for i in range(n_blocks):
        time_ds[i * block_ds_len : (i + 1) * block_ds_len] = time[
            i * block_len : (i + 1) * block_len : ds_factor
        ]

    return time_ds


@click.command()
@click.argument("infile", type=click.Path(exists=True))
@click.option("--outfile", "-o", type=click.Path())
@click.option("--sampling-rate", "-r", type=int, default=100)
def cli(infile, outfile, sampling_rate):

    if outfile is None:
        inpath = Path(infile)
        outfile = inpath.parent / (inpath.stem + ".csv")

    with h5py.File(infile, "r") as hf, open(outfile, "w") as csv_file:
        ds_time = hf["data"]["time"]
        fs_original = 1e9 / ((ds_time[10000] - ds_time[0]) / 10000)
        print(f"original sampling rate: {int(fs_original/1000)}kHz")
        ds_factor = int(fs_original / sampling_rate)
        print(f"downsampling factor: {ds_factor}")

        # Build the csv header, listing all variables
        header = "time,voltage,current"
        csv_file.write(header + "\n")

        data_downsampled = dict()
        # First downsample the time with 'interval' method
        data_downsampled["time"] = downsample_time(
            ds_time[:].astype(float) / 1e9, ds_factor, block_len=100000
        )

        for var in ["voltage", "current"]:
            ds = hf["data"][var]

            # Apply the calibration settings (gain and offset)
            data_downsampled[var] = downsample_signal(
                ds[:] * ds.attrs["gain"] + ds.attrs["offset"],
                ds_factor,
                block_len=100000,
            )
        for i in range(len(data_downsampled["time"])):
            timestamp = datetime.utcfromtimestamp(data_downsampled["time"][i])
            csv_file.write(timestamp.strftime("%Y-%m-%dT%H:%M:%S.%f"))
            # Format and write to the csv file
            for var in ["voltage", "current"]:
                value = data_downsampled[var][i]
                # Write the value to csv
                csv_file.write(f",{value}")
            # Done with this block - terminate line with \n
            csv_file.write("\n")
            if i % 1000 == 0:
                click.echo(
                    f"written {100 * float(i)/len(data_downsampled['time']):.2f}%"
                )


if __name__ == "__main__":
    cli()
