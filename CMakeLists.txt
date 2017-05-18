cmake_minimum_required(VERSION 3.7)
project(HamtTest)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES main.cpp hash_trie.hpp Test_RefCounts.cpp Test_Correctness.cpp Test_Components.cpp Test_Concurrency.cpp)
set(BENCH_FILES bench_main.cpp)

if( CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]" )
    ADD_DEFINITIONS( -DHAMT_DEBUG_RC )
endif()


include_directories( HamtBench . )

# You'll need to update this path to a path to a boost distro on your machine
# (This should probably be done using find_package)
include_directories( HamtBench /Development/ThirdParty/boost_1_64_0 )
add_executable(HamtTest ${SOURCE_FILES})
add_executable(HamtBench ${BENCH_FILES})
target_compile_options( HamtTest PRIVATE -mpopcnt )
