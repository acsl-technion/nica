cmake_minimum_required(VERSION 3.1)

project( ikernels C CXX )

include_directories(hls ../nica/hls)

### Copy and generate PCAP files for the tests
foreach(pcap_file udp_rr.pcap
        memcached-requests.pcap memcached-responses.pcap
        memcached-all-responses.pcap
		memcached-requests-16.pcap memcached-responses-16.pcap
		memcached-all-responses-16.pcap
        ping.pcap pong.pcap
)
    add_custom_target(${pcap_file}
        DEPENDS ${CMAKE_SOURCE_DIR}/ikernels/hls/tests/${pcap_file}
        COMMAND cp ${CMAKE_SOURCE_DIR}/ikernels/hls/tests/${pcap_file} ${pcap_file})
endforeach(pcap_file)

foreach(f pong)
    add_custom_target(${f}-padded.pcap
            COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py ${f}.pcap ${f}-padded.pcap
            DEPENDS ${f}.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py)
endforeach(f)
add_custom_target(udp_rr-reverse.pcap
    COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py udp_rr.pcap udp_rr-reverse.pcap --reverse
    DEPENDS udp_rr.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py)
add_custom_target(udp_rr-to-host.pcap
    COMMAND python ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py udp_rr.pcap udp_rr-to-host.pcap --dest-port 49105
    DEPENDS udp_rr.pcap ${CMAKE_SOURCE_DIR}/nica/hls/tests/pad_small_packets.py)

### Passthrough
add_ikernel(passthrough "hls/passthrough.cpp" "hls/tests/passthrough_tests.cpp" passthrough passthrough_top)

### Threshold
add_ikernel(threshold "hls/threshold.cpp;hls/passthrough.cpp" "hls/tests/threshold_tests.cpp" threshold threshold_top)

### CMS ikernel
add_ikernel(cms "hls/cms-ikernel.cpp;hls/passthrough.cpp;hls/cms.cpp" "hls/tests/cms-ikernel_tests.cpp" cms cms_ikernel)

### Echo
add_ikernel(echo "hls/echo.cpp" "hls/tests/echo_tests.cpp;../nica/hls/tests/tb.cpp" echo echo_top
	"udp_rr.pcap;udp_rr-reverse.pcap;udp_rr-to-host.pcap;ping.pcap;pong-padded.pcap")

### Pktgen
add_ikernel(pktgen "hls/pktgen.cpp;hls/passthrough.cpp" "hls/tests/pktgen_tests.cpp" pktgen pktgen_top)

### Memcached
add_ikernel(memcached "hls/memcached.cpp;hls/passthrough.cpp" "hls/tests/memcached_tests.cpp;../nica/hls/tests/tb.cpp" memcached memcached_top
         "memcached-requests.pcap;memcached-responses.pcap;memcached-all-responses.pcap;memcached-requests-16.pcap;memcached-responses-16.pcap;memcached-all-responses-16.pcap")
set(MEMCACHED_CACHE_SIZE "4096" CACHE STRING
    "Cache size in entries for the memcached ikernel")
set(MEMCACHED_KEY_SIZE "16" CACHE STRING
        "Key size in bytes for the memcached ikernel")
set(MEMCACHED_VALUE_SIZE "16" CACHE STRING
        "Value size in bytes for the memcached ikernel")
foreach(memcached_target memcached_tests memcached-emu)
	target_compile_definitions(${memcached_target} PUBLIC -DMEMCACHED_CACHE_SIZE=${MEMCACHED_CACHE_SIZE}
		-DMEMCACHED_KEY_SIZE=${MEMCACHED_KEY_SIZE}
		-DMEMCACHED_VALUE_SIZE=${MEMCACHED_VALUE_SIZE})
endforeach(memcached_target)

### Add your ikernel here:
# add_ikernel(ikernel sources testbench_sources hls_target_name top_function [testbench_files])

### CMS tests
add_executable(cms_unit_tests EXCLUDE_FROM_ALL hls/tests/cms_tests.cpp hls/cms.cpp)
add_dependencies(check cms_unit_tests)
add_test(cms_unit_tests cms_unit_tests)
add_gtest(cms_unit)

### Heap tests
add_executable(heap_tests EXCLUDE_FROM_ALL hls/tests/heap_tests.cpp)
add_dependencies(check heap_tests)
add_test(heap_tests heap_tests)
add_gtest(heap)

### Cache tests
add_executable(cache_tests EXCLUDE_FROM_ALL hls/tests/cache_tests.cpp)
add_dependencies(check cache_tests)
add_test(cache_tests cache_tests)
add_gtest(cache)

