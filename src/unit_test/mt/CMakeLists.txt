# libprocxx::mt::thread_pool UT
add_executable(thread_pool_ut thread_pool.cxx)
target_link_libraries(thread_pool_ut
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::mt::thread_pool thread_pool_ut)
