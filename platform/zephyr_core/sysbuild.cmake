# This file is included by sysbuild_add_subdirectory()
# (sysbuild_extensions.cmake) after the DEFAULT_IMAGE is registered, with
# SYSBUILD_CURRENT_SOURCE_DIR set inside the function scope — the only context
# where ExternalZephyrProject_Add is valid.

if(SB_CONFIG_NET_CORE_IMAGE_HCI_IPC)
  set(NET_APP hci_ipc)
  set(NET_APP_SRC_DIR ${ZEPHYR_BASE}/samples/bluetooth/${NET_APP})

  externalzephyrproject_add(
    APPLICATION ${NET_APP} SOURCE_DIR ${NET_APP_SRC_DIR} BOARD
    ${SB_CONFIG_NET_CORE_BOARD})
endif()
