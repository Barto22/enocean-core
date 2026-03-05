add_executable(mutex_tests MutexTests.cpp)

set_target_properties(mutex_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(mutex_tests PRIVATE mutex GTest::gtest_main)

gtest_discover_tests(mutex_tests)
