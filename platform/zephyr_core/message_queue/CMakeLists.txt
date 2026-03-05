add_library(message_queue INTERFACE)
target_include_directories(
  message_queue INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET message_queue PROPERTY CXX_STANDARD 23)

target_link_libraries(message_queue INTERFACE imessage_queue platform_core)
