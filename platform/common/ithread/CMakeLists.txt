add_library(ithread INTERFACE)
target_include_directories(
  ithread
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET ithread PROPERTY CXX_STANDARD 23)
target_link_libraries(ithread INTERFACE plog::plog GSL)
target_link_libraries(ithread INTERFACE GSL)
