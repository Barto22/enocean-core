find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

add_library(mutex STATIC Mutex.cpp)
target_include_directories(
  mutex PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_property(TARGET mutex PROPERTY CXX_STANDARD 23)

target_link_libraries(
  mutex
  PUBLIC imutex
  PRIVATE Threads::Threads)
