cmake_minimum_required(VERSION 2.8)
project("libprocxx" CXX)
enable_testing()

# Version
set(LIBPROCXX_VERSION_MAJOR 0)
set(LIBPROCXX_VERSION_MINOR 1)
set(LIBPROCXX_VERSION_PATCH 0)

# Config. header file
configure_file(
    "${PROJECT_SOURCE_DIR}/libprocxx/config.hxx.in"
    "${PROJECT_BINARY_DIR}/libprocxx/config.hxx")

# C++ flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARDREQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Werror -O2 ${CMAKE_CXX_FLAGS} ${CXXFLAGS}")

# Dependencies
find_library(PTHREAD NAMES pthread REQUIRED)
find_library(RT NAMES rt REQUIRED)

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
add_subdirectory(benchmark)
