# libprocxx::mt::thread_pool UT
add_executable(unit_test__libprocxx__mt__thread_pool thread_pool.cxx)
target_link_libraries(unit_test__libprocxx__mt__thread_pool
    LINK_PUBLIC pthread
    LINK_PUBLIC rt)
add_test(libprocxx::mt::thread_pool unit_test__libprocxx__mt__thread_pool)
