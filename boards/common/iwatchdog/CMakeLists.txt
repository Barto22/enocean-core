add_library(iwatchdog INTERFACE)
target_include_directories(
  iwatchdog
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET iwatchdog PROPERTY CXX_STANDARD 23)
target_link_libraries(iwatchdog INTERFACE plog::plog)
