cmake_minimum_required(VERSION 4.0)

if(USE_CLANG)
  set(CMAKE_C_COMPILER "clang")
  set(CMAKE_CXX_COMPILER "clang++")
endif()

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example when
# compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

project(posix_core LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(add_codechecker)
include(add_common_flags)
include(generate_build_info)
include(generate_codecheck_skips)
include(sanitizers)
add_common_flags()

generate_codecheck_skips(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/static_analyse/skip.codecheck
  "${CMAKE_CURRENT_BINARY_DIR}/_deps")

add_codechecker(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/static_analyse
                ${CMAKE_CURRENT_SOURCE_DIR}/config/static_analyse)

add_custom_target(
  vscode_files ALL
  COMMAND
    ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/config/c_cpp_properties.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/c_cpp_properties.json
  COMMAND
    ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../boards/posix/debug/launch.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/launch.json
  COMMAND
    ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../boards/posix/debug/tasks.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/tasks.json)

add_subdirectory(message_queue)
add_subdirectory(mutex)
add_subdirectory(semaphore)
add_subdirectory(sleep)
add_subdirectory(thread)
add_subdirectory(ble)
add_subdirectory(crypto)

set(PLATFORM_POSIX TRUE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../boards/posix
                 ${CMAKE_CURRENT_BINARY_DIR}/boards_posix)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../
                 ${CMAKE_CURRENT_BINARY_DIR}/app_src)

target_link_libraries(application PRIVATE ble crypto sleep)

add_executable(app ${CMAKE_CURRENT_SOURCE_DIR}/posix_main.cpp)
target_link_libraries(
  app
  PRIVATE plog::plog
          GSL
          message_queue
          mutex
          thread
          application
          ble
          crypto
          enocean_driver)

generate_build_info(app ${CMAKE_CURRENT_SOURCE_DIR}/../../)
