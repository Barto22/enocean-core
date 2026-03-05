add_library(imessage_queue INTERFACE)
target_include_directories(
  imessage_queue
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET imessage_queue PROPERTY CXX_STANDARD 23)
target_link_libraries(imessage_queue INTERFACE plog::plog GSL)
