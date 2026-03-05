find_package(PkgConfig REQUIRED)
pkg_check_modules(BLUEZ REQUIRED bluez)

add_library(ble STATIC Ble.cpp)
target_include_directories(
  ble
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  PRIVATE ${BLUEZ_INCLUDE_DIRS})
target_link_libraries(
  ble
  PUBLIC ible thread
  PRIVATE ${BLUEZ_LIBRARIES} plog::plog)
set_property(TARGET ble PROPERTY CXX_STANDARD 23)
target_compile_options(ble PRIVATE ${BLUEZ_CFLAGS_OTHER})
