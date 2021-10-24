# NICA

NICA is a framework for building application-layer inline accelerators for
FPGA-based SmartNICs and using them from server applications.
This is a research prototype; use it at your own risk. It is free to use as a
whole or in parts. Please cite our ATC'19 paper
["NICA: An Infrastructure for Inline Acceleration of Network Applications"](https://www.usenix.org/conference/atc19/presentation/eran).
<details>
  <summary>BibTeX</summary>

    @inproceedings {234884,
    author = {Haggai Eran and Lior Zeno and Maroun Tork and Gabi Malka and Mark Silberstein},
    title = {{NICA}: An Infrastructure for Inline Acceleration of Network Applications},
    booktitle = {2019 {USENIX} Annual Technical Conference ({USENIX} {ATC} 19)},
    year = {2019},
    address = {Renton, WA},
    url = {https://www.usenix.org/conference/atc19/presentation/eran},
    publisher = {{USENIX} Association},
    }

</details>

## What's in here?

* [`nica/`](nica/) - NICA hardware runtime for the FPGA.
* [`ikernels/`](ikernels/) - Implementation of several example AFUs.
* [`manager/`](manager/) - NICA manager daemon implementation.
* [`libnica/`](libnica/) - Application facing library to control NICA AFUs.
* [`scripts/`](scripts/), [`cmake/`](cmake/) - Scripts for building and running NICA.
* [`emulation/`](emulation/) - Wraps NICA and AFU implementation in a library for host emulation.
* [`ntl/`](https://github.com/acsl-technion/ntl) - Networking Template Library, included as a git submodule.
* [`ansible/`](ansible/) - Ansible scripts to set up NICA dependencies.

## What's not here?

* RTL testbench for NICA - our testbench is based on modified testbench from
  Mellanox Innova SDK that we cannot publish.

In addition, some code currently belongs to other repositories:

* Modified memcached to use NICA-KVcache AFU.
* Baseline and modified CoAP server for the authentication AFU.
* Modified `sockperf` tool used for performance measurements.

## Dependencies

NICA's runtime and compile-time dependencies can be installed using an ansible
role provided in the `ansible/` directory. To use them, first 
get the ansible mlnx ofed submodule updated using the command:

```shell
git submodule update --init
```

You can then use the example ansible playbook with the following command:

```shell
    ansible-playbook -i <inventory file> [-l <hostname>] ansible/nica.yml
```

## Building HLS code

### Dependencies

NICA currently builds with Vivado HLS 2018.2.

The tests rely on [googletest](https://github.com/google/googletest). Download
and build it:

```shell
cd ~/workspace
git clone https://github.com/google/googletest --branch v1.8.x
cd googletest
cmake -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON .
make -j
```

The tests also depend on scapy (`python36-scapy` on CentOS) to generate pcap files. 

To get the `ntl` submodule updated use the command:

```shell
git submodule update --init
```

Some of the ikernels rely on additional libraries, such as `openssl-devel` (CoAP).

Assuming this repository is at `~/workspace/nica`, configure it by:

```shell
cd ~/workspace/nica
mkdir build
cd build
cmake -DGTEST_ROOT=~/workspace/googletest/googletest -DNUM_IKERNELS=1 -DBUILD_SOFTWARE=OFF ..
```

Number of ikernels to build can be changed by changing `NUM_IKERNELS` to a
different value. `BUILD_SOFTWARE` is set to `OFF` in order to build just the HLS
code (NICA and example ikernels) without the host applications.

After configuration, it is possible to run the C simulation for all ikernels by
running the `check` target:

```shell
make check
```

It is also possible to build NICA building the `nica` target, and run an RTL/C
co-simulation for that project by building the `nica-sim` target. Other
ikernel targets are listed below.

| ikernel     | target            | RTL/C co-simulation |
| ----------- | ----------------- | -----------------   |
| Passthrough | `passthrough-hls` | `passthrough-sim`   |
| Threshold   | `threshold-hls`   | `threshold-sim`     |
| pktgen      | `pktgen-hls`      | `pktgen-sim`        |
| echo        | `echo-hls`        | `echo-sim`          |
| Top-K       | `cms-hls`         | `cms-sim`           |
| memcached   | `memcached-hls`   | `memcached-sim`     |
| coap        | `coap-hls`        | `coap-sim`          |

## Building host software

This repository can be configured to build the software as well:

```shell
cd ~/workspace/nica/build
cmake -DGTEST_ROOT=~/workspace/googletest/googletest -DNUM_IKERNELS=1 -DBUILD_SOFTWARE=ON ..
make -j
```

## Generating an FPGA image

To run the synthesis and implementation process, you first need to acquire the
Mellanox shell SDK tarball. NICA currently builds against image 2768
(`newton_ku060_40g_v2768.tar`), with Vivado 2018.2. Use the
`prepare-mellanox-shell.sh` script to extract the necessary files and prepare the
build directory:

```shell
cd ~/workspace/nica/build
../scripts/prepare-mellanox-shell.sh newton_ku060_40g_v2768.tar
```

After that the `create_project.tcl` script from the SDK can be used to generate
an image. The script requires environment variables specifying the number of
ikernels, the chosen ikernel, and the build number. For example, to build an
image with the threshold ikernel:

```shell
cd ~/workspace/nica/build/user/project
NUM_IKERNELS=1 \
IKERNEL0=threshold \
vivado -mode batch -source create_project.tcl -tclargs xcku060-ffva1156-2-i flat
```

Check the WNS (worst negative slack) in the timing report under the `reports`
directory to verify that the resulting image meets the timing constraints.

To flash the image on the Innova device, the `gen_flash.tcl` script converts it to the format needed by the
`mlx_fpga` tool, generating the `top.bin` file:

```shell
cd Implement/Impl_flat_sbu
vivado -mode batch -source ../../../scripts/mellanox/gen_flash.tcl
```

# Adding a new ikernel

Sources for example ikernels are availabe under the `ikernels/` directory. For
example, take `ikernels/hls/passthrough.cpp`. In order to add a new ikernel,
create a class derived from the `hls_ik::ikernel` class.
Define a UUID for the new ikernel using the
`uuidgen` tool, and Use the `DEFINE_TOP_FUNCTION` macro to define the HLS top
function for the new ikernel.

You may create a testbench for the ikernel. The example
`ikernels/hls/tests/threshold_tests.cpp` shows a couple of tests for the
`threshold` ikernel, implemented using googletest.

To build the ikernel, add a new `add_ikernel()` call to the
`ikernels/CMakeLists.txt` file with the source files. The function parameters
are documented in comments in the `CMakeLists.txt` file. 

