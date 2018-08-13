# libprocxx::io::socket::address UT
add_executable(sockaddr_ut sockaddr.cxx)
target_link_libraries(sockaddr_ut LINK_PUBLIC libprocxx__io)
add_test(libprocxx::io::sockaddr sockaddr_ut)

# libprocxx::io::event_loop UT (basic)
add_executable(event_loop__basic_ut event_loop.cxx)
target_link_libraries(event_loop__basic_ut
    LINK_PUBLIC libprocxx__io
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::io::event_loop__basic event_loop__basic_ut)

# libprocxx::io::event_loop UT (more advanced)
add_executable(event_loop__tcp_server_ut tcp_server.cxx)
target_link_libraries(event_loop__tcp_server_ut
    LINK_PUBLIC libprocxx__io
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::io::event_loop__tcp_server event_loop__tcp_server_ut)
