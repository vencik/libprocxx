# libprocxx::logging UTs
add_executable(logging__plain_ut plain.cxx)
target_link_libraries(logging__plain_ut
    LINK_PUBLIC libprocxx__logging
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::logging::plain logging__plain_ut)

add_executable(logging__json_ut json.cxx)
target_link_libraries(logging__json_ut
    LINK_PUBLIC libprocxx__logging
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(
    NAME    libprocxx::logging::json
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/json_validate" "./logging__json_ut")

add_executable(logging__aio_ut aio.cxx)
target_link_libraries(logging__aio_ut
    LINK_PUBLIC libprocxx__logging
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::logging::aio logging__aio_ut)
