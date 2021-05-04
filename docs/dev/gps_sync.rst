GPS-Time-Synchronisation
========================

The ultimate goal is to precisely synchronize all Linux system clocks in the network to global time obtained from a GPS receiver.
Note how the following description traverses the clock hierarchy top-down starting with the GPS receiver down to the linux clock on the host node.


General Structure
-----------------

The master node is connected to a u-blox GPS receiver (shepherd-capelet), from which it receives a PPS signal via GPIO and global time via UART if the GPS-Modul has an position-fix.
One kernel module and two services work in concert to provide a grandmaster clock:

- ``pps_gmtimer`` (kernel module) timestamps edges on a GPIO pin with respect to an internal hardware timer on the AM335x and feeds the timestamps as a Linux pps device.
- ``gpsd`` talks to the GPS via a serial interface, parses the protocol and extracts timing information that is fed to the following service (chrony).
- ``chrony`` combines the global timing information from gpsd and the precise PPS signal from the pps device and synchronizes the Linux system clock to global GPS time.


Hardware setup
--------------

The GPS-capelet (see `hardware/capelets/gps`) is stacked on top of the shepherd cape or an attached harvesting capelet, respectively.

- The PPS signal is connected to Timer4 on pin P8_7
- The GPS-Uart is connect over UART2 on pins P9_21 and P9_22
- The currently used gps-chip is the u-blox SAM-M8Q

Configuring GNSS module
-----------------------

The u-blox GPS receiver can be configured to optimize its perfomance. To do this directly form the host node the python the python script: ``ubloxmsg_exchange`` was
written (see https://github.com/kugelbit/ubx-packet-exchange). The configuration files can be found under ``config_files``.  To this end the following configurations were set:

- SBAS was disabled for greater accuracy of the timepulse signal
- The Galileo system was enabled for increased performance

In addtion the standard config of the receiver leads to the following behaviour:

- If the PPS is not locked, the LED on the capelet will not blink. After the lock is attained, the LED will start blinking at 1 Hz.
- NMEA messages are enabled for the UART link which connects to the BeagleBone.

Note: Recent versions of gpsd include a tool `ubxtool`, allowing convenient configuration of ublox receivers:
 - Poll GNSS config: `ubxtool -p CFG-GNSS`
 - Enable Galileo: `ubxtool -e GALILEO`
 - Enable binary messages: `ubxtool -e BINARY`
 - Disable NMEA messages: `ubxtool -d NMEA`
 - Disable SBAS: `ubxtool -d SBAS`
 - Poll time pulse config: `ubxtool -p CFG-TP5`


Deploy
------

Please use the ansible gps-host role.
``ansible-playbook deploy.yml``

Useful commands
---------------

Check if PPS pulses are coming: ``cat /sys/class/pps/pps0/assert``
On the master check that gpsd and chrony are running:

- ``systemctl status gpsd``
- ``systemctl status chrony``

gpsd comes with a command line client that display GPS data on the commandline: ``cgps``
Familiarize with chrony output:

- ``chronyc sources -v``
- ``chronyc sourcestats -v``
- ``chronyc tracking -v``

Configuration files:

- chrony: ``/etc/chrony/chrony.conf``
- gpsd: ``/etc/default/gpsd``
- the device tree file (dts) for the GPS-caplet is found in the pps-gmtimer folder (DD-GPS-00A0.dts).

DEBUG:
if you want to see whats going on you can run the services in DEBUG-Mode:

- chrony: ``sudo chronyd -dd``
- gpsd: ``sudo gpsd -N /dev/ttyO2 -D15 -n``


Pitfalls
--------
- For a good result (fast and stable fixes) it is very import to provide a clear sky view to the gps-caplet.
- The are lot of possible configuration options for the u-blox chip (see  SAM-M8Q Receiver Description) that may furthre optimize performance. For more research in this direction use the u-blox u-center software.
- The currently used u-blox chip does not support the u-blox time mode. This mode can increase the timing accuracy (see u-blox M8 Receiver Description).
