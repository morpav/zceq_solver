cmake_minimum_required(VERSION 3.0)
project(zcash_testing)

set(CMAKE_C_COMPILER "gcc")

#set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_COMPILER "clang++")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -Wall -pedantic -march=native")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -march=native -fprofile-instr-generate=code.profraw")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -std=c++11 -Wall -pedantic -march=native -fprofile-instr-use=code.profdata")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -Wl,--no-export-dynamic")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_LIB_LINKER_FLAGS} -static-libgcc -static-libstdc++")

set(ZCEQ_SOLVER_SOURCES_COMMON
        zceq_solver.cpp
        zceq_blake2b.cpp
        zceq_space_allocator.cpp
        blake2/blake2b-ref.c
        blake2/blake2b-compress-ref.c
        blake2/blake2b-compress-avx2.c
        blake2/blake2b-compress-sse41.c
        blake2/blake2b-compress-ssse3.c
        blake2b-asm/zcblake2_avx1.o
        blake2b-asm/zcblake2_avx2.o
)

set_source_files_properties(
        blake2b-asm/asm/zcblake2_avx1.o
        PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
)

set_source_files_properties(
        blake2b-asm/asm/zcblake2_avx2.o
        PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
)


#add_executable(zcash_testing ${SOURCE_FILES})
# target_link_libraries(zcash_testing)

add_executable(zceq_benchmark
        ${ZCEQ_SOLVER_SOURCES_COMMON}
        benchmark.cpp
)


add_library(zceq_solver_sh SHARED
        ${ZCEQ_SOLVER_SOURCES_COMMON}
        lib_main.cpp)

###########

#include_directories(deps/zcash/src/)
#include_directories(deps/zcash/depends/x86_64-unknown-linux-gnu/include)


#set(ZCEQ_CLI_SOURCES_V0
#        zceq_solver_v0.cpp
#        # zceq_main.cpp
#        zceq_space_allocator.cpp)

#link_directories(../libsodium/src/libsodium/.libs)
#add_executable(zceq_solver_v0 ${ZCEQ_CLI_SOURCES_V0})
#target_link_libraries(zceq_solver_v0
#        libsodium.a
#        libstdc++.a)
