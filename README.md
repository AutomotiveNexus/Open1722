![Status - Incubating](https://img.shields.io/static/v1?label=Status&message=Incubating&color=FEFF3A&style=for-the-badge)

# Open1722

## Introduction

Open1722 is a fork of [AVNU/libavtp](https://github.com/Avnu/libavtp) which is an open source reference implementation of the Audio Video Transport Protocol (AVTP) specified in IEEE 1722-2016 spec. _libavtp_ primarily focuses on audio video data formats of the IEEE 1722-2016 spec.

IEEE 1722 is also gaining a lot of traction in the automotive community, mainly, to bridge fieldbus technologies over automotive Ethernet. In particular the AVTP Control Formats (ACF) specify serialization for a set of data formats relevant for automotive applications (e.g., CAN, LIN, etc.). Open1722 extends/modifies _libavtp_ to also include these ACF formats.

NOTE: Open1722 is currently incubating and under active development. The APIs are not fully stable and are subject to changes.

Open1722 is under BSD License. For more information see LICENSE file.

## Implementation

This repository is organized as follows:
- The `src/` and `include/` folders contain the IEEE 1722 protocol implementation. We strive to make the implementation platform independant and avoid usage of platform specific headers or libraries. For now the implementation is tested only on Linux.
- The `examples/` folder contains various applications that use our Open1722 library. The applications are targeted to Linux platforms.

Before building Open1722 make sure you have installed the following software :
* Meson >= 0.56
* Ninja >= 1.10.1

Alternatively, you can use VS Code to run the provided dev container which takes care of the dependencies.

The first step to build Open1722 is to generate the build system files.
```
$ meson build
```

Then build Open1722 by running the following command. The building artifacts will be created under the build/ in the top-level directory.
```
$ meson compile -C build
```

The build can be cleaned using the following command:
```
$ meson --wipe build
```

To install Open1722 on your system run:
```
$ cd build
$ sudo meson install
```

To build all the AVTP targets from the repository, we have a script:
```
$ ./build_all.sh
```

## AVTP Formats Support

AVTP protocol defines several AVTPDU type formats (see Table 6 from IEEE 1722-2016 spec).

The following is the list of the formats currently supported by Open1722:
 - AAF (PCM encapsulation only)
 - CRF
 - CVF (H.264, MJPEG, JPEG2000)
 - RVF
 - AVTP Control Formats (ACF) (see Table 22 from IEEE 1722-2016 spec)
    - CAN
    - CAN Brief
    - GPC
    - Sensor
    - Sensor Brief

## Examples

The `examples/` directory provides sample applications which demonstrate the Open1722 functionalities. Each example directory contains a README file that includes specific details on its functionality, configuration, and dependencies.

To build an example application run `$ meson compile -C build <path to example>`. On a successful build, the executables are available in the `build/<path to example>`.

E.g. to build and execute the IEEE 1722 CAN Talker application:
```
$ meson compile -C build ./examples/acf-can/acf-can-talker
$ ./build/examples/acf-can/acf-can-talker
```
