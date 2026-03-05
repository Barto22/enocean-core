add_executable(message_queue_tests MessageQueueTests.cpp)

set_target_properties(message_queue_tests PROPERTIES CXX_STANDARD 23)

target_link_libraries(message_queue_tests PRIVATE message_queue
                                                  GTest::gtest_main)
gtest_discover_tests(message_queue_tests)
