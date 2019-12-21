SYSFS interface
===============

The shepherd kernel module provides a user interface that exposes relevant parameters and allows control of the state of the underlying shepherd engine consisting of the kernel module and the firmware running on the two PRU cores. When the module is loaded, the interface is available under `/sys/shepherd`

- `n_buffers`: The maximum number of buffers used in the data exchange protocol
- `samples_per_buffer`: The number of samples contained in one buffer. Each sample consists of a current and a voltage value.
- `buffer_period_ns`: Time period of one 'buffer'. Defines the sampling rate togethere with `samples_per_buffer`
- `memory/address`: Physical address of the shared memory area that contains all `n_buffers` data buffers used to exchange data
- `memory/size`: Size of the shared memory area in bytes
- `calibration_settings`: Load calibration settings. They are used in the virtcap algorithm.
- `virtcap_settings`: Settings which configure the virtcap algorithm.
- `sync/error_sum`: Integral of PID control error
- `sync/error`: Instantaneous PID control error
- `sync/correction`: PRU Clock correction (in ticks~5ns) as calculated by the PID controller
