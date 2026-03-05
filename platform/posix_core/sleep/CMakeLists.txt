add_library(sleep STATIC Sleep.cpp)
target_include_directories(
  sleep PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET sleep PROPERTY CXX_STANDARD 23)

target_link_libraries(
  sleep
  PUBLIC isleep
  PRIVATE platform_core)
