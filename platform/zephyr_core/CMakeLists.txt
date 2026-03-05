cmake_minimum_required(VERSION 4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example when
# compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_core LANGUAGES C CXX ASM)

test_sysbuild()

if(CONFIG_USBD_CDC_ACM_CLASS)
  include(usb_common/common.cmake)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(add_codechecker)
include(generate_build_info)
include(generate_codecheck_skips)

generate_codecheck_skips(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/static_analyse/skip.codecheck
  "${CMAKE_CURRENT_BINARY_DIR}/../mcuboot"
  "${CMAKE_CURRENT_BINARY_DIR}/zephyr"
  "${CMAKE_CURRENT_BINARY_DIR}/_deps"
  "$ENV{ZEPHYR_BASE}"
  "$ENV{ZEPHYR_BASE}/../bootloader"
  "$ENV{ZEPHYR_BASE}/../modules")

add_codechecker(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/static_analyse
                ${CMAKE_CURRENT_SOURCE_DIR}/config/static_analyse)

set(vscode_copy_commands
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/config/c_cpp_properties.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/c_cpp_properties.json)

if(EXISTS "${BOARD_DIR}/debug/launch.json.in")
  list(
    APPEND
    vscode_copy_commands
    COMMAND
    ${CMAKE_COMMAND}
    -E
    copy
    ${BOARD_DIR}/debug/launch.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/launch.json)
endif()

if(EXISTS "${BOARD_DIR}/debug/tasks.json.in")
  list(
    APPEND
    vscode_copy_commands
    COMMAND
    ${CMAKE_COMMAND}
    -E
    copy
    ${BOARD_DIR}/debug/tasks.json.in
    ${CMAKE_CURRENT_SOURCE_DIR}/../../.vscode/tasks.json)
endif()

add_custom_target(vscode_files ALL ${vscode_copy_commands})

add_subdirectory(logging)
add_subdirectory(message_queue)
add_subdirectory(mutex)
add_subdirectory(semaphore)
add_subdirectory(sleep)
add_subdirectory(thread)
add_subdirectory(ble)
add_subdirectory(crypto)

set(PLATFORM_ZEPHYR TRUE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../boards/zephyr
                 ${CMAKE_CURRENT_BINARY_DIR}/boards_zephyr)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../
                 ${CMAKE_CURRENT_BINARY_DIR}/app_src)

target_link_libraries(platform_core INTERFACE zephyr)

target_link_libraries(application PRIVATE ble crypto sleep)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/zephyr_main.cpp)

target_link_libraries(
  app
  PRIVATE zephyr_logger
          application
          message_queue
          mutex
          sleep
          thread
          watchdog
          ble
          crypto
          enocean_driver)
generate_build_info(app ${CMAKE_CURRENT_SOURCE_DIR}/../../)
