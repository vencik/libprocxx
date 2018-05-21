# libprocxx::io::event_loop UT
add_executable(unit_test__libprocxx__io__event_loop event_loop.cxx)
target_link_libraries(unit_test__libprocxx__io__event_loop
    LINK_PUBLIC libprocxx__io
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::io::event_pool unit_test__libprocxx__io__event_loop)
