find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

add_library(message_queue INTERFACE)
target_include_directories(
  message_queue INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET message_queue PROPERTY CXX_STANDARD 23)

target_link_libraries(message_queue INTERFACE imessage_queue platform_core
                                              Threads::Threads)
