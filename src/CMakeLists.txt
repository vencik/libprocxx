cmake_minimum_required(VERSION 2.8)
project("Various algorithms" CXX)
enable_testing()

# Version
set(LIBPROCXX_VERSION_MAJOR 0)
set(LIBPROCXX_VERSION_MINOR 1)
set(LIBPROCXX_VERSION_PATCH 0)

# Config. header file
configure_file(
    "${PROJECT_SOURCE_DIR}/config.hxx.in"
    "${PROJECT_BINARY_DIR}/config.hxx")

# C++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -g -Wall -Werror --std=c++11 -O2 -pthread")

# Strip source directory prefix from file names
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -D__SRCFILE__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

# Top-level include path
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

# Allow inclusion of the config. header file
include_directories("${PROJECT_BINARY_DIR}")

# Subdirectories
add_subdirectory(libprocxx)
add_subdirectory(unit_test)
