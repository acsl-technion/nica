# NICA

Infrastructure for application-layer offloading.

## Building HLS code

### Dependencies

NICA currently builds with Vivado HLS 2016.2.

The tests rely on [googletest](https://github.com/google/googletest). Download
and build it:

```shell
cd ~/workspace
git clone https://github.com/google/googletest    
cd googletest
cmake -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON .
make -j
```

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

## Building host software

Now this repository can be configured to build the software as well:

```shell
cd ~/workspace/nica/build
cmake -DGTEST_ROOT=~/workspace/googletest/googletest -DNUM_IKERNELS=1 -DBUILD_SOFTWARE=ON ..
make -j
```

## Generating an FPGA image

To run the synthesis and implementation process, you first need to acquire the
Mellanox shell SDK tarball. NICA currently builds against image 640
(`newton_ku060_2_40g_v640.tar`), with Vivado 2016.2. Use the
`prepare-mellanox-shell.sh` script to extract the necessary files and prepare the
build directory:

```shell
cd ~/workspace/nica/build
../scripts/prepare-mellanox-shell.sh newton_ku060_2_40g_v640.tar
```

After that the `create_project.tcl` script from the SDK can be used to generate
an image. The script requires environment variables specifying the number of
ikernels, the chosen ikernel, and the build number. For example, to build an
image with the threshold ikernel:

```shell
cd ~/workspace/nica/build/user/project
NUM_IKERNELS=1 \
IKERNEL0=threshold \
vivado -mode batch -source create_project.tcl -tclargs xcku060-ffva1156-2-i
```

Check the WNS (worst negative slack) in the timing report under the `reports`
directory to verify that the resulting image meets the timing constraints.

To flash the image on the Innova device, the `gen_flash.tcl` script converts it to the format needed by the
`mlx_fpga` tool, generating the `top.bin` file:

```shell
cd Implement/Impl_fullchip
vivado -mode batch -source ../../../scripts/mellanox/gen_flash.tcl
```

# Adding a new ikernel

Sources for example ikernels are availabe under the `ikernels/` directory. For
example, take `ikernels/hls/passthrough.cpp`. In order to add a new ikernel,
create a class derived from the `hls_ik::ikernel` class and from the
`hls_ik::gateway_impl` template. Define a UUID for the new ikernel using the
`uuidgen` tool, and Use the `DEFINE_TOP_FUNCTION` macro to define the HLS top
function for the new ikernel.

You may create a testbench for the ikernel. The example
`ikernels/hls/tests/threshold_tests.cpp` shows a couple of tests for the
`threshold` ikernel, implemented using googletest.

To build the ikernel, add a new `add_ikernel()` call to the
`ikernels/CMakeLists.txt` file with the source files. The function parameters
are documented in comments in the `CMakeLists.txt` file. 

