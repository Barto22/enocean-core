add_library(igpio INTERFACE)
target_include_directories(
  igpio
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../include>)
set_property(TARGET igpio PROPERTY CXX_STANDARD 23)
target_link_libraries(igpio INTERFACE plog::plog GSL)
