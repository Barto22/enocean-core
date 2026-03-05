add_executable(error_handler_tests ErrorHandlerTests.cpp)

set_target_properties(error_handler_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(error_handler_tests PRIVATE GTest::gtest_main plog::plog)

include(GoogleTest)
gtest_discover_tests(error_handler_tests)
