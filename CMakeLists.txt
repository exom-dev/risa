cmake_minimum_required(VERSION 3.15)
project(risa C)

set(CMAKE_C_STANDARD 99)

file(GLOB_RECURSE SOURCE_FILES
        src/*.h
        src/*.c)

string(TOLOWER "${CMAKE_BUILD_TYPE}" PROJECT_BUILD_TYPE)

if(PROJECT_BUILD_TYPE MATCHES release OR PROJECT_BUILD_TYPE MATCHES minsizerel)
    message("-- RISA: Compiling for RELEASE")
else()
    message("-- RISA: Compiling for DEBUG")
endif()

if(TARGET_TYPE STREQUAL "library")
    message("-- RISA: Target Type: library")

    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_definitions(-DTARGET_TYPE_LIBRARY)
    list(FILTER SOURCE_FILES EXCLUDE REGEX ".*/src/main.c$")
    add_library(risa SHARED ${SOURCE_FILES})
else()
    message("-- RISA: Target Type: executable")
    add_executable(risa ${SOURCE_FILES})
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(risa PRIVATE -Wall -Wextra)

    set(CMAKE_C_FLAGS_RELEASE FLAGS "${CMAKE_C_FLAGS} -O3")

    if(ARCH STREQUAL "x86")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(risa PRIVATE -W2)

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} /O2")
endif()