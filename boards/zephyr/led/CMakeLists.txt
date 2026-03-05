add_library(led STATIC Led.cpp)
target_include_directories(
  led PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET led PROPERTY CXX_STANDARD 23)

target_link_libraries(led PUBLIC gpio mutex)
