VirtCap
=======

The overall approach of VirtCap emulation is very similar to regular emulation.
Main difference is that the output is not a dac voltage / current,
but switching the LDO output on and off.
The output switch should be connected to `P8_11`.
The algorithm is executed in the pru0 core.

Passing recordings to pru0
--------------------------

On the python side, the VirtCap is initiated in the Emulator class.
With regular emulation, the shared buffers are filled `dac values`.
During VirtCap emulation, the shared buffers are filled with actual
recorded `adc current and voltage values`. Because the emulation and recording
can be done on different devices. The recorded `adc values` are compensated
to use the same calibration settings as the emulation calibration settings.
This is done in `__init__.py`.

Extended sysfs_interface
------------------------

Besides the recorded `adc current and voltage values`, more information needs
to be passed down to the pru0 core. Two new structs have been added to the
`sysfs_interface shared memory`.

.. code-block:: c

    struct CalibrationSettings {
      /* Gain of load current adc. It converts current to adc value */
      int32_t adc_load_current_gain;
      /* Offset of load current adc */
      int32_t adc_load_current_offset;
      /* Gain of load voltage adc. It converts voltage to adc value */
      int32_t adc_load_voltage_gain;
      /* Offset of load voltage adc */
      int32_t adc_load_voltage_offset;
    } __attribute__((packed));

    /* This structure defines all settings of virtcap emulation*/
    struct VirtCapSettings {
      int32_t upper_threshold_voltage;
      int32_t lower_threshold_voltage;
      int32_t sample_period_us;
      int32_t capacitance_uf;
      int32_t max_cap_voltage;
      int32_t min_cap_voltage;
      int32_t init_cap_voltage;
      int32_t dc_output_voltage;
      int32_t leakage_current;
      int32_t discretize;
      int32_t output_cap_uf;
      int32_t lookup_input_efficiency[4][9];
      int32_t lookup_output_efficiency[4][9];
    } __attribute__((packed));

This information is send to the pru0 core at the start of virtcap emulation in
`__init__.py`.

VirtCap Execution
-----------------

After VirtCap is initialized, it gets called every 10us, just as regular
emulation. First it receives input current and input voltage from the shared
memory passed down from the python application. Then it samples output current
and output voltage. To reduce execution time, this is done in a 7:1 ratio. Thus
7 VirtCap iterations in a row, current gets sampled. Then next iteration voltage
gets sampled. This way only 1 ADC sample is required each iteration. If no new
sample is taken, the last measured sample is used.

Then the VirtCap function is called (using the input/output voltage/current as
input for the function). Based on these values, the new storage capacitor
voltage is calculated. The input and output efficiency of the converter is
modelled using lookup tables, as function of the input or output current.

Every iteration the capacitor voltage is checked to not go beyond its minimum
and maximum value. However, the `upper_threshold_voltage` and 
`lower_threshold_voltage` is checked every 'discretize' iterations. This models
the behavior of the BQ255xx, where only the threshold voltages are checked every
~64ms.

Co-operation with messaging
---------------------------

Besides exchanging shared-memory buffers, the python or kernel side also sends
messages to the pru0 core. When such a message arrives, no new sample is taken.
This is to prevent the total execution of overflowing 10us.
