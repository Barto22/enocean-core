add_executable(enocean_driver_tests EnoceanDriverTests.cpp)

set_target_properties(enocean_driver_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(enocean_driver_tests PRIVATE enocean_driver icrypto
                                                   GTest::gtest_main plog::plog)

gtest_discover_tests(enocean_driver_tests)
