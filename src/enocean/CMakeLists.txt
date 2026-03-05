add_library(enocean_driver INTERFACE)
target_include_directories(
  enocean_driver INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)
set_property(TARGET enocean_driver PROPERTY CXX_STANDARD 23)
