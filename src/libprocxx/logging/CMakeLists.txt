# Logging front-end
add_library(libprocxx__logging
    logger.cxx
    plain_formatter.cxx
    json_formatter.cxx
    file_processor.cxx)

target_include_directories(libprocxx__logging
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../")
