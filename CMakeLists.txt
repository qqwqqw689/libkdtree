cmake_minimum_required(VERSION 3.2)
project(libkdtree)

set(CMAKE_CXX_STANDARD 11)

# Find OpenMP

# APPLE
# Set to True when the target system is an Apple platform
# OpenMP_FOUND
# Variable indicating that OpenMP flags for all requested languages have been found.
# message([<mode>] "message text" ...)
# STATUS : The main interesting messages that project users might be interested in.

IF (APPLE)
    message(STATUS "clang does not support OpenMP!")
ELSEIF (WIN32 OR UNIX)
    find_package(OpenMP REQUIRED)
    IF(OpenMP_FOUND)
        message(STATUS "OpenMP Found!")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    ENDIF()
ENDIF ()

# Libkdtree source files
set(SOURCE_FILES src/kdtree.cpp src/kdtree.h)

# A demo
set(DEMO_FILES demo.cpp)

# Build libkdtree

# add_library
# Adds a library target called <name> to be built from the
# source files listed in the command invocation.
# SHARED libraries are linked dynamically and loaded at runtime.
add_library(kdtree SHARED ${SOURCE_FILES})
# Build demo

# add_executable
# Adds an executable target called <name> to
# be built from the source files listed in the command invocation.
add_executable(demo ${DEMO_FILES})

# set_target_properties
# Sets properties on targets.
# LINKER_LANGUAGE
# Specifies language whose compiler will invoke the linker.
set_target_properties(kdtree PROPERTIES LINKER_LANGUAGE C)

# Link lib and demo
target_link_libraries(demo kdtree)