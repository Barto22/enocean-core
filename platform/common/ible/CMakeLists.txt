add_library(ible INTERFACE)
target_include_directories(
  ible
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET ible PROPERTY CXX_STANDARD 23)
