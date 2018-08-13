# libprocxx::io::socket::address UT
add_executable(unit_test__libprocxx__io__sockaddr sockaddr.cxx)
target_link_libraries(unit_test__libprocxx__io__sockaddr
    LINK_PUBLIC libprocxx__io)
add_test(libprocxx::io::sockaddr unit_test__libprocxx__io__sockaddr)

# libprocxx::io::event_loop UT (basic)
add_executable(unit_test__libprocxx__io__event_loop__basic event_loop.cxx)
target_link_libraries(unit_test__libprocxx__io__event_loop__basic
    LINK_PUBLIC libprocxx__io
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::io::event_loop__basic unit_test__libprocxx__io__event_loop__basic)

# libprocxx::io::event_loop UT (more advanced)
add_executable(unit_test__libprocxx__io__event_loop__tcp_server tcp_server.cxx)
target_link_libraries(unit_test__libprocxx__io__event_loop__tcp_server
    LINK_PUBLIC libprocxx__io
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::io::event_loop__tcp_server unit_test__libprocxx__io__event_loop__tcp_server)
