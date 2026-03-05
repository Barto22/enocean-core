add_library(isleep INTERFACE)
target_include_directories(
  isleep INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET isleep PROPERTY CXX_STANDARD 23)
