Getting started
===============

This section describes how to setup an instance of shepherd in a tethered setup.

Prerequisites
-------------

To setup an instance of shepherd, you'll need to assemble a number shepherd nodes.

For each shepherd node, you'll need:

* BeagleBone (Green/Black)
* shepherd cape
* storage capelet

  * for recording: harvesting capelet and transducer, e.g. solar capelt and solar cell
  * for emulation: target capelet

In addition, you'll need at least one SD-card with at least 4GB capacity.

For the cape and capelets take a look at the `hardware design files <https://github.com/geissdoerfer/shepherd/tree/master/hardware>`_
The capelets can easily be soldered by hand.
The shepherd cape has a large number of small components and we suggest to send it to a PCB fab for assembly.

Place a capacitor of desired capacitry on your storage capelets.
The reference layout offers a choice of three footprints allowing to flexibly choose a suitable capacitor and package.

If you don't have the necessary resource or need assistance with getting the hardware manufactured, get in touch with the developers.

To connect the shepherd nodes to each other for control, data collection and time-synchronization, you need to setup an Ethernet network.
The network should be as flat as possible, i.e. have a minimum number of switches.
By default, the BeagleBone Ubuntu image is configured to request an IP address by DHCP.
Therefore your network should have a DHCP server.

Hardware setup
--------------

Stack the cape on top of the BeagleBone.
Make sure to close the jumpers JP1 and JP2 on the cape.

Mount the storage capelet on headers P3/P4 on the cape.

Stack the harvesting capelet on top of the shepherd cape.
The two 11x2 headers of the capelet plug into the lower part (P8_1 - P8_22 and P9_1 - P9_22) of the 23x2 headers of the shepherd cape.
Pay attention to header P1 for the correct orientation.

Stack the target capelet on top of the shepherd cape.
The two 11x2 headers of the capelet plug into the upper part (P8_25 - P8_46 and P9_25 - P9_46) of the 23x2 headers of the shepherd cape.
Pay attention to header P2 for the correct orientation.

Provide all BeagleBones with power through the micro USB ports and connect their Ethernet ports to an Ethernet switch.
Using a PoE switch and corresponding micro USB power splitters can greatly reduce the cabling requirements.

The DHCP server and your machine (for installation/control) must be connected to the same network.


Installation
------------

Prepare the SD-cards.
If you plan to install the OS and shepherd software on the onboard EMMC flash, you can prepare one SD card and sequentially flash the nodes.
If you plan to install the OS and shepherd software on SD card, you have to prepare one SD card for every shepherd node.
Depending on your choice, follow `the official instructions <https://elinux.org/BeagleBoardUbuntu>`_ for **BeagleBone**.
Shepherd has been tested on Ubuntu 18.04 LTS, but might work with other Debian based distributions.

After installing the OS on the BeagleBones and booting them, find out their IP addresses.
If you know the subnet, you can use nmap from your machine, for example:

.. code-block:: bash

    nmap 192.168.178.0/24

Clone the shepherd repository to your machine:

.. code-block:: bash

    git clone https://github.com/geissdoerfer/shepherd.git


Add an inventory file in the `inventory` folder in the repository, assigning hostnames to the IP addresses of the shepherd nodes.
Just start by editing the provided `inventory/herd.yml` example.
Pick a username that you want to use to login to the nodes and assign as `ansible_user` variable.

.. code-block:: yaml

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

We'll use `Ansible <https://www.ansible.com/>`_ to roll out a basic configuration to the nodes.
This includes setting the hostname, adding the user, allowing password-less ssh access and sudo without password.
Make sure that you have `Python >=3.6`, `pip` and `sshpass` installed on your machine.
Install `Ansible` with:

.. code-block:: bash

    pip3 install ansible

Now run the *bootstrap* `Ansible playbook <https://docs.ansible.com/ansible/latest/user_guide/playbooks_intro.html>`_ using the previously prepared inventory file:

.. code-block:: bash

    ansible-playbook deploy/bootstrap.yml

To streamline the installation and upgrading process, the shepherd software is packaged and distributed as debian packages.
Installing is as easy as adding the shepherd repository to the aptitude sources and installing the shepherd metapackage.
The *install* playbook allows to easily automate this process on a group of nodes.

.. code-block:: bash

    ansible-playbook deploy/install.yml

To install and configure PTP for time-synchronization, you can set the `ptp` variable on the command line:

.. code-block:: bash

    ansible-playbook deploy/install.yml -e "ptp=True"


On success, the nodes will reboot and should be ready for use, for example, using the *shepherd-herd* command line utility.
