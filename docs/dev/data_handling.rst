Data handling
=============

Data Acquisition
----------------

Data is sampled/replayed through the ADC (TI ADS8694) and DAC (TI DAC8562T). Both devices are interfaced over a custom, SPI-compatible protocol. For a detailed description of the protocol and timing requirements, refer to the corresponding datasheets. The protocol is bit-banged using the low-latency GPIOs connected to PRU1. The transfer routines are implemented in assembly.

PRU to host
-----------

Data is sampled and transferred between PRUs and user space in buffers, i.e. blocks of SAMPLES_PER_BUFFER samples. These buffers correspond to sections of a continuous area of memory in DDR RAM to which both PRU and user space application have access. This memory is provisined through remoteproc, a Linux framework for managing resources in an AMP (asymmetric multicore processing) system. The PRU firmware contains a so-called resource table that allows to specify required resources. We request a carve-out memory area, which is a continuous, non-cached area in physical memory. On booting the PRU, the remoteproc driver reads the request, allocates the memory and writes the starting address of the allocated memory area to the resource table, which is readable by the PRU during run-time. The PRU exposes this memory location through shared RAM, which is accessible through the sysfs interface provided by the kernel module. Knowing physical address and size, the user space application can map that memory after which it has direct read/write access. The total memory areas is divided into N_BUFFERS distinct buffers.

In the following we describe the data transfer process for emulation. Emulation is the most general case because harvesting data has to be transferred from database to the analog frontend, while simultaneously consumption data (target voltage and current) has to be transferred from the analog frontend (ADC) to the database.

The userspace application writes the first block of harvesting data into one of the buffers, e.g. buffer index i. After the data is written, it sends an RPMSG to PRU1, indicating the message type (MSG_DEP_BUF_FROM_HOST) and index (i). The PRU receives that message and stores the index in a ringbuffer of empty buffers. When it's time, the PRU retrieves a buffer index from the ringbuffer and, sample by sample reads the harvesting values (current and voltage) from the buffer, sends it to the DAC and subsequently samples the 'load' ADC channels (current and voltage), overwriting the harvesting samples in the buffer. Once the buffer is full, the PRU sends an RPMSG with message type (MSG_DEP_BUF_FROM_PRU) and index (i). The userspace application receives the index, reads the buffer, writes its content to the database and fills it with the next block of harvesting data for emulation.

Data extraction
---------------

The user space code (written in python) has to extract the data from a buffer in the shared memory. Generally, a user space application can only access its own virtual address space. We use Linux's `/dev/mem` and python's `mmap.mmap(..)` to map the corresponding region of physical memory to the local address space. Using this mapping, we only need to seek the memory location of a buffer, extract the header information using `struct.unpack()` and interpret the raw data as numpy array using `numpy.frombuffer()`.


Database
--------

In the current implementation, all data is locally stored on SD-card. This means, that for emulation, the harvesting data is first copied to the corresponding shepherd node. The sampled data (harvesting data for recording and load data for emulation) is also stored on each individual node first and later copied and merged to a central database. We use the popular HDF5 data format to store data and meta-information.
