Command-line tools
==================

Shepherd offers two command line utilities:

`Shepherd-herd`_ is the command line utility for remotely controlling a group of shepherd nodes.
This is the key user interface to shepherd.
The pure-python package is installed on the user's local machine and sends commands to the shepherd nodes over *ssh*.

`Shepherd-sheep`_ is the command line utility for locally controlling a single shepherd node.
Depending on your use-case you may not even need to directly interact with it!

.. _shepherd-herd-cli:

shepherd-herd
-------------

.. click:: shepherd_herd:cli
   :prog: shepherd-herd
   :show-nested:

Examples
********

In the following we assume that you have an ansible style, YAML-formatted inventory file named `hosts` in your current working diretory.
Refer to the example `hosts` file in the root directory of the shepherd repository.
To start recording harvesting data using the hardware MPPT algorithm and store it under the default path, overwriting existing data and precharging the capacitor before starting the recording:

.. code-block:: bash

    shepherd-herd -i hosts record -f --init-charge

To stop recording on a subset of nodes (sheep1 and sheep3 in this example):

.. code-block:: bash

    shepherd-herd -i hosts -l sheep1,sheep3 stop

Another example for recording, this time with a fixed harvesting voltage of 600mV, limited recording duration of 30s and storing the result under `/var/shepherd/recordings/rec_v_fixed.h5`.
Also, instead of using the inventory file option, we specify hosts and ssh user on the command line:

.. code-block:: bash

    shepherd-herd -i sheep0,sheep1, -u jane record -d 30 --harvesting-voltage 0.6 -o rec_v_fixed.h5

To retrieve the recordings from the shepherd nodes and store them locally on your machine under the `recordings/` directory:

.. code-block:: bash

    shepherd-herd -i hosts retrieve -d rec_v_fixed.h5 recordings/

Before turning to emulation, here's how to flash a firmware image to the attached sensor nodes.
To flash the image `firmware.bin` that is stored on the local machine:

.. code-block:: bash

    shepherd-herd -i hosts target flash firmware.bin

To reset the CPU on the sensor nodes:

.. code-block:: bash

    shepherd-herd -i hosts target reset


Finally, to replay previously recorded data from the file `rec.h5` in the shepherd node's file system and store the recorded IV data from the load as `load.h5`:

.. code-block:: bash

    shepherd-herd -i hosts emulate -o load.h5 rec.h5

.. _shepherd-sheep-cli:

shepherd-sheep
--------------

.. click:: shepherd.cli:cli
   :prog: shepherd-sheep
   :show-nested:


Examples
********

Coming soon...
