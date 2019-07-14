Basics
======

*shepherd* is a testbed for the battery-less Internet of Things.
It allows to record harvesting conditions at multiple points in space over time.
The recorded data can be replayed to attached wireless sensor nodes, examining their behaviour under the constraints of spatio-temporal energy availability.

For a detailed description see our paper [TODO]

A *shepherd* instance consists of a group of spatially distributed *shepherd* nodes that are time-synchronized with each other.
Each *shepherd* node consists of a BeagleBone, the *shepherd* cape and a particular choice of capelets according to the user requirements.

Shepherd works in two key modes: `Recording`_ and `Emulation`_.
Through various digitally controlled analog switches, we can reconfigure the hardware according to the selected mode from software.


Time-synchronization
--------------------

Generally, shepherd can be used on a single node or without time-synchronization, for example, to measure the amount of energy that can be harvested in a particular scenario.
The more interesting feature however is that it enables to explore harvesting conditions across time and space, which is a crucial step towards collaboration and coordination of battery-less nodes.

For tethered settings, we propose to use the `Precision Time Protocol (PTP) <https://en.wikipedia.org/wiki/Precision_Time_Protocol>`_.
It requires that all shepherd nodes are connected to a common Ethernet network with as little switching jitter as possible.
The BeagleBone has hardware support for PTP and Linux provides the necessary tools.

In mobile or long-range scenarios, it might not be feasible to connect the nodes with Ethernet.
Instead, you can use the timing signal from a GPS receiver.
We have designed a GPS capelet (hardware/capelets/gps) to easily connect a GPS receiver to shepherd, however we have not yet tested it and there is no software support yet.
Get in touch with one of the developers if you need GPS support for your project!


Recording
---------

For recording, shepherd nodes are equipped with a harvesting transducer, e.g., a solar panel or piezo-electric harvester.
This transducer is connected to the input of the DC/DC boost converter on the shepherd cape.
The capacitor-buffered output of the DC/DC converter is connected to a *dummy load* that discharges the capacitor as soon as a threshold voltage is reached.
This is important, because the DC/DC converter can only charge a capacitor up to a maximum voltage before it switches off.
Shepherd measures the voltage and current on the input of the DC/DC converter.

A group of shepherd nodes can be deployed to the environment of interest to measure the energy that can be harvested at each node.
The time-synchronization between the shepherd nodes allows to gather the readings from multiple nodes with respect to a common time-line.
The data thus represents the spatio-temporal energy availability.

An important aspect of recording is the operating point of the DC/DC converter.
A harvesting transducer is characterized by its IV-curve, determining the magnitude of current at a specific load voltage.
The DC/DC boost converter can (within limits) regulate the input to a desired voltage, by adapting the current drawn from the transducer.
Thus, we have to inform the converter at which voltage we want to operate the transducer.
There are two options:

`Maximum Power Point Tracking (MPPT) <https://en.wikipedia.org/wiki/Maximum_Power_Point_Tracking>`_
***************************************************************************************************

The BQ25505 boost converter on the shepherd cape implements a simple MPPT algorithm in hardware.
In short, it disables the input every 16 seconds, samples the open circuit voltage and regulates its input voltage to a hardware-configurable fraction of that open circuit voltage.
Shepherd accounts for this by exposing the sampling input of the DC/DC converter to the harvesting capelet, such that the user can configure the voltage divider according to the specific type of harvester.
The low sampling rate of the algorithm makes it suitable only for slowly-changing harvesting conditions like solar energy.

User provided reference voltage
*******************************

A user can tell shepherd the voltage with which it should operate the harvester during recording.
Currently, the software only supports setting a constant voltage at the beginning of the recording.

Emulation
---------

In emulation mode, spatio-temporal current and voltage data is replayed to a group of sensor nodes.
Each shepherd node hosts a DAC controlled current source that can precisely set the current into the DC/DC converter.
The DC/DC converter has a reference voltage input that we use to set the voltage on its input.
With these two mechanisms, we are able to force the DC/DC converter into an arbitrary operating point.
Relying on time-synchronization, shepherd can thus faithfully reproduce previously recorded (or model-based) spatio-temporal energy conditions.

Instead of the dummy load, the capacitor-bufferd output of the DC/DC converter is connected to the target sensor node.
Thus, the sensor node is operated under the constraints of emulated energy availability.

Like other testbeds, shepherd records the voltage on the capacitor and the current consumed by the sensor node during emulation.
Furthermore, four GPIO lines and one bi-directional UART are level-translated between shepherd and the attached sensor node, allowing to trace

Remote programming/debugging
----------------------------

For convenient debugging and development, shepherd implements a fully functional Serial-Wire-Debug (SWD) debugger.
SWD is supported by most recent ARM Cortex-M corand allows flashing images and debugging the execution of code.
Older platforms typically provide a serial boot-loader, which can be used to flash images over the pre-mentioned UART connection.
