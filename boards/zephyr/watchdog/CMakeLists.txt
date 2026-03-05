add_library(watchdog STATIC Watchdog.cpp)
target_include_directories(
  watchdog PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET watchdog PROPERTY CXX_STANDARD 23)

target_link_libraries(
  watchdog
  PUBLIC iwatchdog
  PRIVATE platform_core)
