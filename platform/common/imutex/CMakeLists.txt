add_library(imutex INTERFACE)
target_include_directories(
  imutex
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET imutex PROPERTY CXX_STANDARD 23)
target_link_libraries(imutex INTERFACE plog::plog GSL)
