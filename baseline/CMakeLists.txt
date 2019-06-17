cmake_minimum_required(VERSION 3.1)
find_package(Boost COMPONENTS system program_options REQUIRED)
find_package(ibverbs REQUIRED)

find_path(NICA_HLS_DIR nica-top.hpp
    HINTS
        ../nica/hls/
)

find_path(THRESHOLD_IKERNEL_DIR threshold.hpp
    HINTS
        ../ikernels/hls/
)

include(FindPkgConfig)
pkg_check_modules(UUID REQUIRED uuid)

include_directories(${UDP_INCLUDE_DIR} ${THRESHOLD_IKERNEL_DIR} ${NICA_HLS_DIR}
    ${UUID_INCLUDE_DIRS} ${XILINX_VIVADO_HLS} ${IBVERBS_INCLUDE_DIRS} ../libnica)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(threshold_server ThresholdServerMain.cpp RunnableServerBase.cpp
    StatisticsUdpServer.cpp UdpServer.cpp)
add_executable(cms_server CmsServerMain.cpp RunnableServerBase.cpp
    CmsUdpServer.cpp UdpServer.cpp ../ikernels/hls/cms.cpp)
add_executable(echo_server EchoServerMain.cpp RunnableServerBase.cpp
    EchoUdpServer.cpp UdpServer.cpp UdpClient.cpp)

add_executable(client RunnableUdpClient.cpp UdpClient.cpp)

foreach(executable threshold_server cms_server echo_server client)
	target_link_libraries(${executable} Threads::Threads)
	target_link_libraries(${executable} ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY})
	target_link_libraries(${executable} nica)
	target_link_libraries(${executable} ${UUID_LIBRARIES})
	target_link_libraries(${executable} ${IBVERBS_LIBRARIES})
	target_compile_features(${executable} PRIVATE cxx_rvalue_references cxx_generalized_initializers)
endforeach(executable)
