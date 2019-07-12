Calibration
===========

Data recorded with shepherd are stored in the database as raw binary values, read from the ADC.
In order to make sense of that data, we need to map the binary values to physical voltage and current values.
Similarly, for emulation, we need to map phyiscal voltage and current values to binary values that can be fed to the DAC.

This mapping is assumed to be linear.
For example, to convert a binary value :math:`v_{binary}` to its physical equivalent :math:`v_{physical}`:

.. math::

    v_{physical} = v_{binary} \cdot gain + offset


The *gain* and *offset* values can be derived from nominal hardware configuration.
However, due to component tolerances, offset voltages and other effects, the real mapping can significantly differ from nominal values.

Therefore, we recommend to *calibrate* shepherd by comparing a number of sample points recorded with shepherd to known refernce values measured with accurate lab equipment.
From the tuples of reference values and measurements, you can estimate the *gain* and *offset* by ordinary least squares estimation.
