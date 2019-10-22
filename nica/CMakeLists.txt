cmake_minimum_required(VERSION 3.1)

project( nica C CXX )

include_directories(hls ../ikernels/hls ../ikernels/hls/tests)

foreach(pcap_file passthrough.pcap input.pcap
        0bad.pcap f00d.pcap
)
    add_custom_target(${pcap_file}
        DEPENDS ${CMAKE_SOURCE_DIR}/nica/hls/tests/${pcap_file}
        COMMAND cp ${CMAKE_SOURCE_DIR}/nica/hls/tests/${pcap_file} ${pcap_file})
endforeach(pcap_file)

set(nica_sources hls/nica.cpp hls/udp.cpp hls/mlx.cpp hls/flow_table.cpp
    hls/custom_rx_ring.cpp hls/ikernel.cpp)
set(nica_testbench_sources hls/tests/main.cpp hls/tests/tb.cpp
    ../ikernels/hls/passthrough.cpp ../ikernels/hls/threshold.cpp ../ikernels/hls/pktgen.cpp
    )
set(nica_pcap_files passthrough.pcap input.pcap
    passthrough-padded.pcap
    input-padded.pcap all_sizes.pcap all_sizes-padded.pcap
    0bad.pcap 0bad-padded.pcap f00d.pcap
    f00d-padded.pcap
    input-bth.pcap)

add_custom_target(pcap_files ALL DEPENDS ${nica_pcap_files})

# HLS
hls_target(nica nica "${nica_sources}" "${nica_testbench_sources};${nica_pcap_files}" nica)

# NICA
add_library(nica-csim ${nica_sources})
set_property(TARGET nica-csim PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_features(nica-csim PRIVATE cxx_auto_type)
target_link_libraries(nica-csim pcap)

add_executable(nica_tests EXCLUDE_FROM_ALL ${nica_testbench_sources})
add_dependencies(check nica_tests)
target_compile_features(nica_tests PRIVATE cxx_auto_type)
target_link_libraries(nica_tests pcap nica-csim ${GTEST_LIBRARIES} Threads::Threads)
add_dependencies(nica_tests pcap_files)

foreach(f passthrough input all_sizes 0bad)
    add_custom_command(OUTPUT ${f}-padded.pcap
            COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py ${f}.pcap ${f}-padded.pcap
            DEPENDS ${f}.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py)
endforeach(f)
add_custom_command(OUTPUT all_sizes.pcap
    COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/gen_packets.py
    DEPENDS hls/tests/gen_packets.py)
add_custom_command(OUTPUT f00d-padded.pcap
    COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py f00d.pcap f00d-padded.pcap --dest-port 2989
    DEPENDS f00d.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py)
foreach(f input)
    add_custom_command(OUTPUT ${f}-bth.pcap
            COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/add_bth.py ${f}.pcap ${f}-bth.pcap
            DEPENDS ${f}.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/add_bth.py
	    ${CMAKE_SOURCE_DIR}/nica/hls/tests/roce.py)
endforeach(f)

add_test(NAME nica_tests COMMAND nica_tests)

### add hash table / flow table tests
add_executable(flow_table_tests EXCLUDE_FROM_ALL
    hls/tests/flow_table_tests.cpp)
add_dependencies(check flow_table_tests)
add_test(flow_table_tests flow_table_tests)
add_gtest(flow_table)

add_executable(pfifo_tests EXCLUDE_FROM_ALL hls/tests/pfifo_tests.cpp)
add_dependencies(check pfifo_tests)
add_test(pfifo_tests pfifo_tests)
add_gtest(pfifo)

add_executable(arbiter_tests EXCLUDE_FROM_ALL hls/tests/arbiter_tests.cpp hls/arbiter.cpp hls/demux.cpp)
add_dependencies(check arbiter_tests)
add_test(arbiter_tests arbiter_tests)
add_gtest(arbiter)
