add_library(ble STATIC Ble.cpp)
target_include_directories(
  ble PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ble PUBLIC ible zephyr plog::plog)
set_property(TARGET ble PROPERTY CXX_STANDARD 23)
