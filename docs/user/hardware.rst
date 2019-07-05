Hardware
========

Shepherd is a HW/SW solution and this section describes the key interfaces of the shepherd HW that are relevant to users.

Each shepherd node consists of 5 key components:
 - The BeagleBone is a Single Board Computer, handling synchronization, data storage and hardware control
 - The shepherd cape hosts the DC/DC converter, amplifiers and ADC, DAC and corresponding emulation circuitry and all other fixed hardware parts
 - The harvesting capelet connects a harvesting transducer (e.g. solar panel) to the input of the DC/DC converter on the shepherd cape
 - The target capelet connects a sensor node (e.g. a microcontroller with radio) to the capacity buffered output of the DC/DC converter on the shepherd cape
 - The storage capelet hosts the capacitor used to temporarily store harvested energy during recording and emulation

Harvesting capelet
------------------

The harvesting capelet connects any type of harvester, e.g., a solar panel or piezo-electric element to the shepherd hardware.
By keeping this part of the hardware separate from the main shepherd cape, we allow to easily connect various types of harvesters, without having to change the complex and expensive main cape.

The harvesting capelet connects to the shepherd cape via three headers.
Two headers connect directly to the lower half of the pins of the P8 and P9 headers on the BeagleBone.
The third header is the custom, 4-pin connection between the capelet and the cape.

There are two reference harvesting capelets, one for solar harvesting (hardware/capelets/solar) and one for kinetic harvesting (hardware/capelets/kinetic).

.. table:: Header P1 pinout

    +----------+-----------------------------------------+
    |Pin number|Description                              |
    +==========+=========================================+
    |1         |Ground                                   |
    |2         |Ground                                   |
    |3         |Input of DC/DC converter                 |
    |4         |Input for setting MPP via voltage divider|
    +----------+-----------------------------------------+

Target capelet
--------------

Shepherd provides a generic interface to connect any type of sensor node that can be supplied by emulated energy traces, while tracing its state with GPIO and UART.

The target capelet connects to the shepherd cape via three headers.
Two headers connect directly to the upper half of the pins of the P8 and P9 headers on the BeagleBone.
The third header is the custom, 16-pin connection between the capelet and the cape.

.. table:: Header P2 pinout

    ========== ====================================================================================
    Pin number Description
    ========== ====================================================================================
    1          Capacity buffered output of DC/DC converter
    2          Ground
    3          Battery voltage in operating range indicator
    4          High side of shunt resistor for harvesting current measurement
    5          +3.3V from BeagleBone LDO, use for any additional infrastructure circuitry
    6          Low side of shunt resistor for harvesting current measurement
    7          Ground
    8          GPIO2 - voltage translated digital signal from target to BeagleBone for tracing
    9          UART TX - voltage translated UART signal from target to Beaglebone UART1
    10         GPIO0 - voltage translated digital signal from target to BeagleBone for GPIO tracing
    11         UART RX - voltage translated UART signal from Beaglebone UART1 to target
    12         GPIO3 - voltage translated digital signal from target to BeagleBone for tracing
    13         SWDCLK - Clock line for remote programming via ARM Serial-Wire-Debug (SWD)
    14         GPIO1 - voltage translated digital signal from target to BeagleBone for tracing
    15         SWDIO - Data line for remote programming via ARM Serial-Wire-Debug (SWD)
    16         Ground
    ========== ====================================================================================

Pin 5 provides regulated 3.3V to supply any additional circuitry independent of the actual harvesting power supply.
This could be used to power, for example, a debugging LED that should not interfere with the energy emulation/measurement.
Pins 4 and 6 provide access to the voltage across the shunt resistor between the harvester/emulator and the DC/DC converter.
This allows to measure harvesting voltage and current online on the sensor node, which might be interesting for certain directions of research.
Pins 9 and 11 expose the level-translated signals to UART1 on BeagleBone. They can be used to trace UART messages from the target sensor node.
Pins 8, 10, 12, 14 are (level-translated) connected to the low-latency GPIOs that are sampled by shepherd software for GPIO tracing.
Pins 13 and 15 (level-translated) are used to program/debug a connected sensor node with SWD.

Storage capelet
---------------

Although less relevant for recording, the choice of capacitor crucially defines system behaviour during emulation.
Therefore, shepherd allow to easily swap the capacitor to experiment with different technologies/capacities.

The storage capelet directly connects the two ends of a mounted capacitor to the corresponding traces on the shepherd cape via two custom, 4-pin headers.

========== ===========================================================
Pin number Description
========== ===========================================================
1          +3.3V from BeagleBone LDO, use for any additional circuitry
2          High side of capacitor
3          Ground
4          Ground
========== ===========================================================