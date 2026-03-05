add_executable(semaphore_tests SemaphoreTests.cpp)

set_target_properties(semaphore_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(semaphore_tests PRIVATE semaphore GTest::gtest_main)

gtest_discover_tests(semaphore_tests)
