add_library(thread STATIC Thread.cpp)
target_include_directories(
  thread PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET thread PROPERTY CXX_STANDARD 23)

target_link_libraries(
  thread
  PUBLIC ithread
  PRIVATE platform_core)
