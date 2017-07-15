# libprocxx::logging UTs
add_executable(unit_test__libprocxx__logging__plain plain.cxx)
target_link_libraries(unit_test__libprocxx__logging__plain
    LINK_PUBLIC libprocxx__logging)
add_test(libprocxx::logging::plain unit_test__libprocxx__logging__plain)

add_executable(unit_test__libprocxx__logging__json json.cxx)
target_link_libraries(unit_test__libprocxx__logging__json
    LINK_PUBLIC libprocxx__logging)
add_test(
    NAME    libprocxx::logging::json
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/json_validate"
        "./unit_test__libprocxx__logging__json")
