add_library(gpio STATIC Gpio.cpp)
target_include_directories(
  gpio PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET gpio PROPERTY CXX_STANDARD 23)

target_link_libraries(
  gpio
  PUBLIC igpio
  PRIVATE plog::plog)
