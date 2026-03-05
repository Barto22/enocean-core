target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/usb_common)
target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/usb_common/common_usbd.c)
