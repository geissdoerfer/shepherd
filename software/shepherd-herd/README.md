# shepherd-herd

*shepherd-herd* ist the command line utility for controlling a group of shepherd nodes remotely through an IP-based network.


## Installation

*shepherd-herd* is a pure python package and available on PyPI.
Use your python package manager to install it.
For example, using pip:

```
pip install shepherd-herd
```

## Usage

All *shepherd-herd* commands require the list of hosts on which to perform the requested action.
This list of hosts is provided with the `-i` option, that takes either the path to a file or a comma-separated list of hosts (compare Ansible `-i`).

For example, save the following file as `hosts`.

```
sheep:
  hosts:
    sheep0:
    sheep1:
    sheep2:
  vars:
    ansible_user: jane
```

Then use shepherd-herd to check if all your nodes are up:

```
shepherd-herd -i hosts run echo 'hello'
```

Or, equivalently define the list of hosts on the command line

```
shepherd-herd -i sheep0,sheep1,sheep2, run echo 'hello'
```

Here, we just provide a selected set of examples of how to use *shepherd-herd*.
For a full list of supported commands and options, run ```shepherd-herd --help``` and ```shepherd-herd [COMMAND] --help```.

Simultaneously start a 30s recording on the nodes defined in the `hosts` file:

```
shepherd-herd -i hosts record -o rec.h5
```

After recording is done, retrieve the data from all nodes and merge it to one hdf5 file on your local machine for analysis:

```
shepherd-herd -i hosts retrieve --merge -o rec_merged.h5 rec.h5
```

Flash a firmware image to the sensor nodes attached to the shepherd nodes:

```
shepherd-herd -i hosts target flash firmware_img.bin
```

Reset the sensor nodes:

```
shepherd-herd -i hosts target reset
```

Simultaneously start to play back the previously recorded data to the attached sensor nodes and monitor their power consumption and GPIO events:

```
shepherd-herd -i hosts emulate -i rec.h5 -o load.h5
```
