# Emulation library
add_library(nica-emu SHARED emu-top.cpp)
include_directories(../nica/hls ../ikernels/hls)
target_link_libraries(nica-emu nica-csim)
set(ikernels threshold passthrough pktgen memcached coap)
foreach(ikernel ${ikernels})
	target_link_libraries(nica-emu ${ikernel}-emu)
endforeach(ikernel)
target_compile_features(nica-emu PRIVATE cxx_constexpr)
