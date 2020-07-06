cmake_minimum_required(VERSION 3.15)
project(risa C)

set(CMAKE_C_STANDARD 99)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS_RELEASE FLAGS "${CMAKE_C_FLAGS} -O3")

    if(ARCH STREQUAL "x86")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} /O2")
endif()

message(STATUS "Compiling for ${ARCH}")

file(GLOB SOURCE_FILES
        src/*.h
        src/*.c

        src/**/*.h
        src/**/*.c)

add_executable(risa ${SOURCE_FILES} src/debug/disassembler.h src/debug/disassembler.c src/value/value.h src/value/value.c src/value/value_array.c src/vm/vm.h src/vm/vm.c src/common/def.h src/vm/vm_stack.c)