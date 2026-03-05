add_library(isemaphore INTERFACE)
target_include_directories(
  isemaphore
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET isemaphore PROPERTY CXX_STANDARD 23)
target_link_libraries(isemaphore INTERFACE plog::plog GSL)
