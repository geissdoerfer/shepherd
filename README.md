# SHEpHERD: SyncHronized Energy Harvesting Emulator RecorDer

[![Build Status](https://travis-ci.org/geissdoerfer/shepherd.svg?branch=master)](https://travis-ci.org/geissdoerfer/shepherd)

*shepherd* is a testbed for the battery-less Internet of Things.
It allows to record harvesting conditions at multiple points in space over time.
The recorded data can be replayed to attached wireless sensor nodes, examining their behaviour under the constraints of spatio-temporal energy availability.

For a detailed description see our paper [TODO]

A *shepherd* instance consists of a group of spatially distributed *shepherd* nodes that are time-synchronized with each other.
Each *shepherd* node consists of a BeagleBone, the *shepherd* cape and a particular choice of capelets according to the user requirements.

This repository contains the hardware design files for the shepherd cape and the various capelets, the software running on each *shepherd* node as well as the tool to orchestrate a group of *shepherd* nodes connected to a network.

**Features**

 - High-speed, high resolution current and voltage sensing
 - Technology-agnostic: Currently, solar and kinetic energy harvesting are supported
 - Remote programming/debugging of ARM Cortex-M MCUs using Serial-Wire-Debug
 - High resolution, synchronized GPIO tracing
 - Configurable, constant voltage power supply for attached sensor nodes
 - Level-translated serial connection to the attached sensor nodes


## Installation

Start by assembling your *shepherd* nodes, consisting of a BeagleBone Green/Black, a *shepherd* cape, a harvesting capelet and a target capelet.
The next step is to manually install the latest Ubuntu Linux on each BeagleBone.
You can install it to SD-card or the on-board EMMC flash, following [the official instructions](https://elinux.org/BeagleBoardUbuntu).

The following instructions describe how to install the *shepherd* software on a group of *shepherd* nodes connected to an Ethernet network.
We assume that you know the IP address of each node and that your local machine is connected to the same network.

If you haven't done it yet, clone this repository to your local machine:

```
git clone https://github.com/geissdoerfer/shepherd.git
```

Next, install the tools used for installing and controlling the *shepherd* nodes.
We'll use [Ansible](https://www.ansible.com/) to remotely roll out the basic configuration to each *shepherd* node and *shepherd-herd* to orchestrate recording/emulation across all nodes.
The tools are hosted on `PyPI` and require Python version `>=3.6`.
Use your Python dependency manager to install the following two tools.
For example, using `pip`:

```
pip install ansible shepherd-herd
```

Edit the `hosts` file in the root directory of the repository using your favorite text editor, assigning host names and known IP addresses of your BeagleBones.
You can arbitrarily choose and assign the hostnames (sheep0, sheep1, in this example) and the ansible_user.

```
sheep:
  hosts:
    sheep0:
        ansible_host: 192.168.1.100
    sheep1:
        ansible_host: 192.168.1.101
    sheep2:
        ansible_host: 192.168.1.102
  vars:
    ansible_user: jane
```

Now run the `bootstrap.yml` *Ansible* playbook, which sets the hostname, creates a user and enables passwordless ssh and sudo:

```
ansible-playbook -i hosts deploy/bootstrap.yml
```

Finally, use the `install.yml` playbook to setup the *shepherd* software, optionally configuring PTP for time-synchronization:

```
ansible-playbook -i hosts deploy/install.yml -e ptp=true
```


## Usage

Record two minutes of data:

```
shepherd-herd -i hosts record -l 120 recording.h5
```

Retrieve the data to analyze it on your local machine:

```
shepherd-herd -i hosts retrieve recording.h5
```

Finally, replay the previously recorded data to the attached sensor nodes, recording their power consumption:

```
shepherd-herd -i hosts emulate -o consumption.h5 recording.h5
```

## Documentation

The documentation is hosted on [readthedocs](https://shepherd-testbed.readthedocs.io/en/latest/).

## People

*shepherd* is being developed at the Networked Embedded Systems Lab at TU Dresden as part of the DFG-funded project Next-IoT.

The following people have contributed to *shepherd*:

 - [Kai Geissdoerfer](https://www.researchgate.net/profile/Kai_Geissdoerfer)
 - [Mikolaj Chwalisz](https://www.tkn.tu-berlin.de/team/chwalisz/)
 - [Marco Zimmerling](https://wwwpub.zih.tu-dresden.de/~mzimmerl/)
