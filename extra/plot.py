import h5py
import numpy as np
import statistics
import matplotlib.pyplot as plt
import matplotlib
from pathlib import Path
import click
from scipy.signal import decimate


class CountOnOffTime:
    LOW_TO_HIGH_THRESHOLD = 1.5 # volt
    MED_THRESHOLD = 1.5 # volt
    HIGH_TO_LOW_THRESHOLD = 1.5 # volt

    def __init__(self):
        self.is_first_sample = True
        self.pulse_state = False
        self.sample_counter = 0
        self.time_last_change = 0
        self.last_on_pulse = 0

    def detect_transition(self, voltage):
        return_value = 0

        if self.is_first_sample is True: # check if this is the first sample
            if voltage > CountOnOffTime.MED_THRESHOLD: # check whether we start from a HIGH or LOW state
                self.pulse_state = True
            self.is_first_sample =  False
        else:
            transition_occured = False # flag to check whether a transition has occurred
            if self.pulse_state is True:
                if voltage < CountOnOffTime.HIGH_TO_LOW_THRESHOLD:
                    transition_occured = True # high to low transition

            if self.pulse_state is False:
                if voltage > CountOnOffTime.LOW_TO_HIGH_THRESHOLD: 
                    transition_occured = True # low to high transition

            if transition_occured:
                pulse_time = self.sample_counter - self.time_last_change # calculate time passed since last transition
                return_value = (self.pulse_state, pulse_time)

                self.pulse_state = not self.pulse_state # update pulse state
                self.time_last_change = self.sample_counter # update time last change

        self.sample_counter += 1
        return return_value

    def analyse_array(self, data):
        on_pulses = []
        off_pulses = []
        for value in data:
            transition = self.detect_transition(value) # check for duty cycle
            if transition is not 0: # transition detected
                pulse_state = transition[0]
                pulse_time = transition[1]

                if pulse_state is True: # high transition detected
                    self.last_on_pulse = pulse_time # store on_time
                else: # low transition detected
                    if self.last_on_pulse is not 0: # only record if first a on_pulse was detected
                        on_pulses.append(self.last_on_pulse)
                        off_pulses.append(pulse_time)
                        # print(f"adding pulse, ontime: {self.last_on_pulse}, offtime: {pulse_time}")

        return statistics.mean(on_pulses), statistics.mean(off_pulses)


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


def analyse_results(data):
    indexes_on = np.where(data["voltage"] > 1)
    amount_on = len(data["voltage"][indexes_on])
    mean_on_volt = np.mean(data["voltage"][indexes_on])
    mean_on_current = np.mean(data["current"][indexes_on])
    indexes_off = np.where(data["voltage"] <= 1)
    amount_off = len(data["voltage"][indexes_off])

    on_percentage = round(amount_on / data["voltage"].size * 100, 2)
    off_percentage = round(amount_off / data["voltage"].size * 100, 2)

    counter = CountOnOffTime()
    ontime, offtime = counter.analyse_array(data["voltage"].tolist())

    print(mean_on_volt, mean_on_current)
    print(f"ontime: {ontime} ({on_percentage}%), offtime: {offtime} ({off_percentage}%)")

@click.command(short_help="Plot shepherd data from DIR")
@click.option("--directory", "-d", type=click.Path(exists=True))
@click.option("--filename", "-f", type=click.Path(), default="rec.h5")
@click.option("--sampling-rate", "-s", type=int, default=1000)
@click.option("--analyse", "-a", is_flag=True)
@click.option("--do-plot", "-p", is_flag=True)
def cli(directory, filename, sampling_rate, analyse, do_plot):

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

        if analyse:
          analyse_results(data)

    else:
        data = dict()
        pl_dir = Path(directory)


        for child in pl_dir.iterdir():
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

    if (do_plot):
      plt.show()


if __name__ == "__main__":
    cli()
