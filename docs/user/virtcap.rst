VirtCap
=======

Emulation
---------

In addition to regular emulation, shepherd provides VirtCap.
VirtCap uses an algorithm to completely virtualize the DC/DC converter and storage capacitor.
With VirtCap, users emulate any DC/DC converter.
Also the capacitor size is software configurable.
This allows to do a test sweep over various capacitor sizes, 
without having to hand replace them by hand.
VirtCap is configured by means of yml-file.

Example usage:

.. code-block:: bash

    sepherd-sheep emulate --config virtcap_settings.yml /var/shepherd/recordings/rec.0.h

Settings
--------
This is an example of a virtcap-settings file which emulates a BQ25570:

.. code-block:: yaml

    virtcap:
      upper_threshold_voltage: 3500
      lower_threshold_voltage: 3200
      sample_period_us: 10
      capacitance_uf: 1000 
      max_cap_voltage: 4200
      min_cap_voltage: 0 
      init_cap_voltage: 3200
      dc_output_voltage: 2300
      leakage_current: 9
      discretize: 5695
      output_cap_uf: 10
      lookup_input_efficiency: [
        [2621,3276,3440,3522,3522,3522,3522,3563,3604],
        [3624,3686,3727,3768,3768,3768,3788,3788,3788],
        [3788,3788,3788,3788,3788,3809,3809,3788,3727],
        [3706,3706,3645,3604,3481,3481,3481,3481,3481]
      ]
      lookup_output_efficiency: [
        [4995,4818,4790,4762,4735,4735,4708,4708,4681],
        [4681,4681,4681,4654,4654,4654,4654,4654,4602],
        [4551,4551,4602,4708,4708,4708,4708,4654,4654],
        [4602,4654,4681,4654,4602,4628,4628,4602,4602]
      ]


:upper_threshold_voltage: Defines the upper threshold capacitor voltage (in millivolts)
:lower_threshold_voltage: Defines the lower threshold capacitor voltage (in millivolts)
:sample_period_us: Defines the sample period of the algorithm, should be left unchanged
:capacitance_uf: Size of capacitor which is emulated (in microfarad)
:max_cap_voltage: Capacitor voltage will never reach above this voltage (in millivolts)
:min_cap_voltage: Capacitor voltage will never fall below this voltage (in millivolts)
:init_cap_voltage: Initial capacitor voltage (in millivolts)
:dc_output_voltage: Output voltage when output is on (in millivolts). Note that for now, VirtCap can only emulate a configurable fixed output voltage
:leakage_current: Static capacitor leakage current (in micro amperes)
:discretize: Period at which the upper and lower threshold voltages are checked (defined is steps of 10us). The BQ255xx defines a period of ~64ms in its datasheet. Leave at 1 if you want to check the threshold continuous
:output_cap_uf: Simulates an output capacitor which will cause an instant voltage drop in storage capacitor voltage when output turns on.
:lookup_input_efficiency: 
    Defines the input efficiency of the DC/DC converter as function of input current on a logarithmic scale.
    
    | First row defines input efficiency for 0.01--0.09mA.
    | Second row defines input efficiency for 0.1--0.9mA.
    | Third row defines input efficiency for 1--9mA.
    | Fourth row defines input efficiency for 10--90mA.
:lookup_output_efficiency: 
    Defines the output efficiency of the DC/DC converter as function of output current on a logarithmic scale.

    | First row defines output efficiency for 0.01--0.09mA.
    | Second row defines output efficiency for 0.1--0.9mA.
    | Third row defines output efficiency for 1--9mA.
    | Fourth row defines output efficiency for 10--90mA.

Recording
---------
The algorithm assumes that the input power traces, is recorded with the same converter as the converter you are trying to emulate.
This means that if only BQ255xx devices can be emulated using the recording option in Shepherd. It is however possible to record the input power trace with another device, and then convert those readings in the format of Shepherd (.h5).
