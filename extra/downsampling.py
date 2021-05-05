import numpy as np
import h5py
from scipy import signal


def downsample_signal(
    dataset: h5py.Dataset, ds_factor: int, block_len: int = None
):
    if block_len is None:
        # Aim for 100 blocks
        block_len = int(len(dataset) / 100)

    block_ds_len = int(block_len / ds_factor)
    block_len = block_ds_len * ds_factor
    n_blocks = int(len(dataset) / block_len)

    # 8th order chebyshev filter for downsampling
    flt = signal.iirfilter(
        N=8,
        Wn=1 / ds_factor,
        btype="lowpass",
        output="sos",
        ftype="cheby1",
        rp=3,
    )

    # filter state
    z = np.zeros((flt.shape[0], 2))

    sig_ds = np.empty((block_ds_len * n_blocks,))
    for i in range(n_blocks):
        y, z = signal.sosfilt(
            flt, dataset[i * block_len : (i + 1) * block_len], zi=z
        )

        sig_ds[i * block_ds_len : (i + 1) * block_ds_len] = y[::ds_factor][
            :block_ds_len
        ]
        print(f"block {i+1}/{n_blocks} done")
    return sig_ds


def downsample_time(time, ds_factor: int, block_len: int = 1000000):
    if block_len is None:
        # Aim for 100 blocks
        block_len = int(len(time) / 100)

    block_ds_len = int(block_len / ds_factor)
    block_len = block_ds_len * ds_factor
    n_blocks = int(len(time) / block_len)

    time_ds = np.empty((block_ds_len * n_blocks,))
    for i in range(n_blocks):
        time_ds[i * block_ds_len : (i + 1) * block_ds_len] = time[
            i * block_len : (i + 1) * block_len : ds_factor
        ][:block_ds_len]

    return time_ds
