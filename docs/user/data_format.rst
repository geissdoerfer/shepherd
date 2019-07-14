Data format
===========

Data is stored in the popular `Hierarchical Data Format <https://en.wikipedia.org/wiki/Hierarchical_Data_Format>`_.

This section describes the structure of data recorded with shepherd:

.. code-block:: text

    .
    |-- attributes
    |   `-- mode
    |-- data
    |   |-- time
    |   |-- current
    |   |   `-- attributes
    |   |       |-- gain
    |   |       `-- offset
    |   `-- voltage
    |       `-- attributes
    |           |-- gain
    |           `-- offset
    `-- gpio
        |-- time
        `-- values


The "mode" attribute allows to distinguish between "load" data and "harvesting" data.

The data group contains the actual IV data.
Time stores the time for each sample in nanoseconds.
Current and voltage store the binary data as acquired from the ADC.
They can be converted to their physical equivalent using the corresponding gain and offset attributes.
See also :doc:`calibration`.

The gpio group stores the timestamp when a GPIO edge was detected and the corresponding bit mask in values.
For example, assume that all are were low at the beginning of the recording.
At time T, GPIO pin 2 goes high.
The value 0x04 will be stored together with the timestamp T in nanoseconds.

