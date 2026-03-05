cmake_minimum_required(VERSION 4.0)

project(enocean_core LANGUAGES NONE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ccache)
include(add_common_flags)
add_common_flags()

include(FetchContent)
FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
  GIT_TAG master
  EXCLUDE_FROM_ALL FIND_PACKAGE_ARGS)

FetchContent_Declare(
  gsl
  GIT_REPOSITORY https://github.com/Microsoft/GSL
  GIT_TAG main
  EXCLUDE_FROM_ALL FIND_PACKAGE_ARGS)

FetchContent_MakeAvailable(plog gsl)

get_target_property(_plog_inc_dirs plog::plog INTERFACE_INCLUDE_DIRECTORIES)
if(_plog_inc_dirs)
  target_include_directories(plog SYSTEM INTERFACE ${_plog_inc_dirs})
endif()
unset(_plog_inc_dirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(platform/common)
add_subdirectory(boards/common)
add_subdirectory(application)
add_subdirectory(src)

# Unit tests are only supported on POSIX platform with native GCC/Clang Embedded
# platforms (Zephyr) should not build tests
if(BUILD_TESTING AND PLATFORM_POSIX)
  message(STATUS "Building unit tests for POSIX platform")

  enable_testing()
  include(CTest)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_TAG main
    GIT_REPOSITORY https://github.com/google/googletest.git)
  FetchContent_MakeAvailable(googletest)

  target_compile_options(gtest PRIVATE -Wno-double-promotion -Wno-float-equal)

  include(GoogleTest)

  add_subdirectory(test)
elseif(BUILD_TESTING AND NOT PLATFORM_POSIX)
  message(
    WARNING
      "BUILD_TESTING is enabled but tests are only supported on POSIX platform. Ignoring."
  )
endif()
