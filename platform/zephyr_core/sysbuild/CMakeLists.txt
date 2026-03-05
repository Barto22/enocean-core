get_filename_component(MCUBOOT_ABS_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

set(mcuboot_APPLICATION_CONFIG_DIR
    "${MCUBOOT_ABS_PATH}/mcuboot"
    CACHE INTERNAL "MCUboot configuration directory")

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES NONE)
