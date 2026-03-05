add_executable(thread_tests ThreadTests.cpp)

set_target_properties(thread_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(thread_tests PRIVATE thread GTest::gtest_main)

gtest_discover_tests(thread_tests)
