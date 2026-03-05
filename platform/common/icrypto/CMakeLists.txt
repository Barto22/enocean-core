add_library(icrypto INTERFACE)
target_include_directories(
  icrypto
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET icrypto PROPERTY CXX_STANDARD 23)
target_link_libraries(icrypto INTERFACE enocean_driver)
