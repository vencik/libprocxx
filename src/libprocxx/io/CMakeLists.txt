# I/O
add_library(libprocxx__io
    file_descriptor.cxx
    pipe.cxx
    event_loop.cxx)

target_include_directories(libprocxx__io
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../")
