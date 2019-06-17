# Emulation library
find_package(Boost COMPONENTS thread filesystem REQUIRED)
find_package(ibverbs REQUIRED)
add_library(nica SHARED nica.cpp)
include_directories(${UUID_INCLUDE_DIRS})
target_link_libraries(nica ${IBVERBS_LIBRARIES} ${UUID_LIBRARIES} ${Boost_THREAD_LIBRARY} ${Boost_FILESYSTEM_LIBRARY})
target_compile_features(nica PRIVATE cxx_rvalue_references)
