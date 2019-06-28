Basics
======

Shepherd is a testbed for the battery-less IoT. It enables experimentation
with networking solutions under constraints of energy availability.

Shepherd consists of hardware and software. The custom hardware interfaces
the software running on a BeagleBone with the physical world by measuring
harvested energy and replaying it to an attached sensor node. The BeagleBones
are time-synchronized with each other in order to allow synchronized
recording and emulation.

Shepherd works in two key modes: recording and emulation

Recording
---------

For recording, shepherd nodes are equipped with a harvesting transducer,
e.g., a solar panel or piezo-electric harvester. The nodes are deployed to the
environment of interest and record the voltage and current of the transducer,
while a dummy load consumes the energy. As all nodes are synchronized, the
recordings can be time-stamped with respect to a common time-line. This allows
to gather the readings from multiple nodes in one data-set on one time-line.

Emulation
---------

In emulation mode, each shepherd nodes is connected to a sensor node, which
might together form a network, running a user-provided protocol. Shepherd
replays voltage and current over time, charging a small capacitor, from which
the sensor node can draw energy. The emulated voltage and current values can
be read from a previously recorded data-set or arbitrarily generated from,
e.g., a model of a particular energy environment. Similar to recording, the
values are replayed synchronously across all shepherd nodes.

Like other testbeds, shepherd records the voltage on the capacitor and
the current consumed by the sensor node during emulation. Furthermore, 4
bi-directional GPIO lines and one bi-directional UART are level-translated
between shepherd and the attached sensor node, allowing to trace debug outputs,
measure latency, or emulate events.

Remote programming/debugging
----------------------------

For convenient debugging and development, shepherd implements a fully
functional SWD debugger. SWD is supported by most recent ARM Cortex-M cores
and allows flashing images and debugging the execution of code. Older platforms
typically provide a serial boot-loader, which can be used to flash images over
the pre-mentioned UART connection.

To get started, the user starts the OpenOCD service on the
BeagleBone, which talks to the target sensor nodes using two GPIO lines. The
user can remotely connect to the OpenOCD server and start a GDB session from
his/her local machine.

Getting started
---------------

 - Download the latest ubuntu image for BeagleBone
 - Flash to SD-card using dd or the script provided with the image
 - Upgrade all packages and make yourself comfortable by setting a hostname, user and password-less ssh access and sudo
