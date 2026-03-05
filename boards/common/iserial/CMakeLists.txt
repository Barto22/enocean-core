add_library(iserial INTERFACE)
target_include_directories(
  iserial
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET iserial PROPERTY CXX_STANDARD 23)
target_link_libraries(iserial INTERFACE plog::plog)
