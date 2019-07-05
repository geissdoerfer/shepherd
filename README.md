# SHEpHERD: SyncHronized Energy Harvesting Emulator RecorDer

[![Build Status](https://travis-ci.com/geissdoerfer/shepherd.svg?token=FcGuqzEQVKohWmUUbLAD&branch=master)](https://travis-ci.com/geissdoerfer/shepherd)

*shepherd* is a testbed for the battery-less Internet of Things.
It allows to record harvesting conditions at multiple points in space over time.
The recorded data can then be replayed to attached wireless sensor nodes, examining their behaviour under the constraints of spatio-temporal energy availability.

For a detailed description see our paper [TODO]

A *shepherd* instance consists of a group of spatially distributed *shepherd* nodes that are time-synchronized with each other.
Each *shepherd* node consists of a BeagleBone, the *shepherd* cape and a particular choice of capelets according to the user requirements.

This repository contains the hardware design files for the shepherd cape and the various capelets, the software running on each *shepherd* node as well as the tool to orchestrate a group of *shepherd* nodes connected to a network.


## Installation

The first step is to manually install Ubuntu Linux on each BeagleBone.
You can install it to SD-card or the on-board EMMC flash, following the instructions [the official instructions](https://elinux.org/BeagleBoardUbuntu).

The following instructions describe how to install the *shepherd* software on a group of *shepherd* nodes connected to an Ethernet network.
We assume that you know the IP address of each node and that your local machine is connected to the same network.

If you haven't done it already, clone this repository to your machine:

```
git clone https://github.com/geissdoerfer/shepherd.git
```

Next, install the tools used for installing and controlling the *shepherd* nodes.
The tools are hosted on `PyPI` and require Python verions `>=3.6`.
Use your Python dependency manager to install the following two tools.
For example, using `pip`:

```
pip install ansible shepherd-herd
```

Edit the `hosts` file in the root directory of the repository using your favorite text editor, assigning host names and known IP addresses of your BeagleBones.
You can arbitrarily choose and assign the hostnames here and 

```
sheep:
  hosts:
    beaglebone0:
        ansible_host: 192.168.1.100
    beaglebone1:
        ansible_host: 192.168.1.101
    beaglebone2:
        ansible_host: 192.168.1.102
  vars:
    ansible_user: joe
```

The `bootstrap.yml` playbook sets the hostname, creates a user and enables passwordless ssh and sudo. Run it with:

```
ansible-playbook -i hosts software/install/bootstrap.yml
```

Finally, we use ansible to setup the *shepherd* software, optionally configuring PTP for time-synchronization:

```
ansible-playbook -i hosts software/install/deploy.yml
```


## Usage

Record two minutes of data:

```
shepherd-herd -u joe hosts record -l 120 --f recording.h5
```

Retrieve the data to analyze it on your local machine:

```
shepherd-herd -u joe hosts retrieve recording.h5
```

Finally, replay the previously recorded data to the attached sensor nodes, recording their power consumption:

```
shepherd-herd -u joe hosts emulate -o consumption.h5 recording.h5
```

## Features

*shepherd* has some additional features, making it a convenient, versatile tool:

 - Remote programming/debugging of ARM Cortex-M MCUs using Serial-Wire-Debug
 - High resolution, synchronized GPIO tracing
 - Configurable, constant voltage power supply for attached sensor nodes
 - Level-translated serial connection to the attached sensor nodes

## Documentation

Read the docs here.

## People

*shepherd* is being developed at the Networked Embedded Systems Lab at TU Dresden as part of the DFG-funded project Next-IoT.

The following people have contributed to *shepherd*:

[Kai Geissdoerfer](https://www.researchgate.net/profile/Kai_Geissdoerfer)
Mikolaj Chwalisz
[Marco Zimmerling](https://wwwpub.zih.tu-dresden.de/~mzimmerl/)
