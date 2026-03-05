add_library(semaphore STATIC Semaphore.cpp)
target_include_directories(
  semaphore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET semaphore PROPERTY CXX_STANDARD 23)

target_link_libraries(semaphore PUBLIC isemaphore platform_core)
