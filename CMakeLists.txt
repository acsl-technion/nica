cmake_minimum_required(VERSION 3.1)

# Use link_directories relative to the source dir.
cmake_policy(SET CMP0015 NEW)

# Configure path to modules (for find_package)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -Wno-unused-label")

option(BUILD_SOFTWARE "Build software running on the host" ON)
set(NUM_IKERNELS 1 CACHE STRING "Number of ikernels to support")
set(NUM_TC 4 CACHE STRING "Number of traffic classes")

add_definitions(-DNUM_IKERNELS=${NUM_IKERNELS} -DNUM_TC=${NUM_TC})

set(GTEST_ROOT "$ENV{GTEST_ROOT}" CACHE PATH "Root directory of gtest installation")
find_package(GTest REQUIRED)

set(XILINX_VIVADO_VERSION "$ENV{XILINX_VIVADO_VERSION}" CACHE STRING
    "Version of Vivado to use (2016.2 or 2016.4)")

find_path(XILINX_VIVADO_HLS
    "include/hls_stream.h"
    HINTS /opt/Xilinx/Vivado_HLS/${XILINX_VIVADO_VERSION}
          /opt/Xilinx/Vivado/${XILINX_VIVADO_VERSION}
    PATHS /opt/Xilinx/Vivado_HLS/2016.2
          /opt/Xilinx/Vivado_HLS/2016.4
    DOC "Xilinx Vivado HLS include directory")

find_path(TOE_INCLUDE
    "toe/toe.hpp"
    HINTS ../../../fpga-network-stack/hls/
          ../../fpga-network-stack/hls/
    DOC "FPGA system's group TCP/IP stack HLS directory")

set(XILINX_VIVADO_HLS_INCLUDE "${XILINX_VIVADO_HLS}/include")

include(FindPkgConfig)
pkg_check_modules(UUID REQUIRED uuid)

find_program(TCPDUMP tcpdump)
get_filename_component(TCPDUMP_PATH ${TCPDUMP} DIRECTORY)

enable_testing()
# add a check target to build all tests and run them
# see https://cmake.org/Wiki/CMakeEmulateMakeCheck
add_custom_target(check COMMAND env PATH=${TCPDUMP_PATH}:$$PATH ${CMAKE_CTEST_COMMAND})

include_directories(
  ${GTEST_INCLUDE_DIRS}
  ${UUID_INCLUDE_DIRS}
  ${XILINX_VIVADO_HLS_INCLUDE}
  ${TOE_INCLUDE}
)

# pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(BUILD_SOFTWARE)
    add_subdirectory(libnica)
    add_subdirectory(baseline)
endif()

function(hls_target proj hls_target_name sources testbench_sources top_function)
    string(REPLACE ";" " " sources "${sources}")
    string(REPLACE ";" " " testbench_sources "${testbench_sources}")
    string(CONCAT cmd "sed "
	    "-e \"s#@IKERNEL@#${proj}#g\" "
	    "-e \"s#@IKERNEL_TOP@#${top_function}#g\" "
	    "-e \"s#@IKERNEL_SOURCES@#${sources}#g\" "
	    "-e \"s#@IKERNEL_TESTBENCH_SOURCES@#${testbench_sources}#g\" "
	    "-e \"s#@CMAKE_SOURCE_DIR@#${CMAKE_SOURCE_DIR}#g\" "
	    "-e \"s#@CMAKE_CURRENT_SOURCE_DIR@#${CMAKE_CURRENT_SOURCE_DIR}#g\" "
	    "${CMAKE_SOURCE_DIR}/ikernels/ikernel.tcl.in")
    add_custom_target(${proj}.tcl
        COMMAND bash -c ${cmd} > ${proj}.tcl
        DEPENDS ${CMAKE_SOURCE_DIR}/ikernels/ikernel.tcl.in
    )
    add_custom_target(${hls_target_name}-hls
        COMMAND env PATH=${TCPDUMP_PATH}:$$PATH GTEST_ROOT=${GTEST_ROOT}
            NUM_IKERNELS=${NUM_IKERNELS}
            NUM_TC=${NUM_TC}
            MEMCACHED_CACHE_SIZE=${MEMCACHED_CACHE_SIZE}
            MEMCACHED_KEY_SIZE=${MEMCACHED_KEY_SIZE}
            MEMCACHED_VALUE_SIZE=${MEMCACHED_VALUE_SIZE}
            TOE_INCLUDEDIR=${TOE_INCLUDE}
            ${XILINX_VIVADO_HLS}/bin/vivado_hls
            ${proj}.tcl
        DEPENDS ${proj}_tests ${proj}.tcl ${CMAKE_SOURCE_DIR}/nica/ikernel.tcl nica-csim
    )
    add_custom_target(${hls_target_name}-sim
        COMMAND env PATH=${TCPDUMP_PATH}:$$PATH GTEST_ROOT=${GTEST_ROOT}
            NUM_IKERNELS=${NUM_IKERNELS}
            NUM_TC=${NUM_TC}
            MEMCACHED_CACHE_SIZE=${MEMCACHED_CACHE_SIZE}
            MEMCACHED_KEY_SIZE=${MEMCACHED_KEY_SIZE}
            MEMCACHED_VALUE_SIZE=${MEMCACHED_VALUE_SIZE}
            SIMULATION_BUILD=1
            ${XILINX_VIVADO_HLS}/bin/vivado_hls ${proj}.tcl
        DEPENDS ${proj}_tests ${proj}.tcl ${CMAKE_SOURCE_DIR}/nica/ikernel.tcl nica-csim
    )
endfunction(hls_target)

function(add_gtest name)
    target_link_libraries(${name}_tests ${GTEST_LIBRARIES} ${UUID_LIBRARIES})
    target_compile_features(${name}_tests PRIVATE cxx_lambdas)
    target_link_libraries(${name}_tests Threads::Threads)
    target_link_libraries(${name}_tests nica-csim)
endfunction(add_gtest)

### Adds a new ikernel.
#   ikernel - the name of the ikernel
#   sources - sources for HLS synthesis and emulation
#   testbench_sources - sources for HLS C testbench (gtest unit tests)
#   hls_target_name - HLS make target for this ikernel
#   top_function - HLS top function
# optional arguments:
#   testbench_files [ARGV5] - files the testbench requires
function(add_ikernel ikernel sources testbench_sources hls_target_name top_function)
    # Emulator library (also linked with the test)
    add_library(${ikernel}-emu ${sources} ../nica/hls/ikernel.cpp)
    target_compile_features(${ikernel}-emu PRIVATE cxx_constexpr)
    set_property(TARGET ${ikernel}-emu PROPERTY POSITION_INDEPENDENT_CODE ON)

    # Test executable
    add_executable(${ikernel}_tests EXCLUDE_FROM_ALL ${testbench_sources})
    target_link_libraries(${ikernel}_tests ${ikernel}-emu)
    if (ARGC GREATER 5)
	add_dependencies(${ikernel}_tests ${ARGV5})
    endif (ARGC GREATER 5)
    add_dependencies(check ${ikernel}_tests)
    add_test(NAME ${ikernel}_tests COMMAND ${ikernel}_tests)
    add_gtest(${ikernel})
    hls_target("${ikernel}" "${hls_target_name}" "${sources};../nica/hls/ikernel.cpp" "${testbench_sources};${ARGV5}" "${top_function}")
endfunction(add_ikernel)

add_subdirectory(nica)
add_subdirectory(ikernels)
add_subdirectory(emulation)
add_subdirectory(manager)
