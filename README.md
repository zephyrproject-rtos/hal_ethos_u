# Ethos-u Core Driver

This repository contains a device driver for the Ethos-u NPU.

## Building

The source code comes with a CMake based build system. The driver is expeced
to be cross compiled for any of the supported Arm Cortex-m CPUs, which will
require the user to setup a custom toolchain file.

The user is also required to define `CMAKE_SYSTEM_PROCESSOR` for the target CPU,
for example cortex-m55+nodsp+nofp. This can be done either in the toolchain
file or on the command line.

```
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=<toolchain> -DCMAKE_SYSTEM_PROCESSOR=cortex-m<nr><features>
$ make
```
