/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PLATFORM_ZEPHYR_CORE_USB_COMMON_COMMON_USBD_H
#define PLATFORM_ZEPHYR_CORE_USB_COMMON_COMMON_USBD_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <zephyr/usb/usbd.h>

/*
 * The scope of this header is limited to use in USB samples together with the
 * new experimental USB device stack, you should not use it in your own
 * application. However, you can use the code as a template.
 */

/**
 * @brief Initialize and configure USB device
 *
 * This function uses Kconfig.sample_usbd options to configure and initialize a
 * USB device. It configures sample's device context, default string
 * descriptors, USB device configuration, registers any available class
 * instances, and finally initializes USB device. It is limited to a single
 * device with a single configuration instantiated in sample_usbd_init.c, which
 * should be enough for a simple USB device sample.
 *
 * @param msg_cb Message callback function
 * @return Configured and initialized USB device context on success, NULL
 * otherwise
 */
struct usbd_context* sample_usbd_init_device(usbd_msg_cb_t msg_cb);

/**
 * @brief Setup USB device without initialization
 *
 * This function is similar to sample_usbd_init_device(), but does not
 * initialize the device. It allows the application to set additional features,
 * such as additional descriptors.
 *
 * @param msg_cb Message callback function
 * @return Configured USB device context on success, NULL otherwise
 */
struct usbd_context* sample_usbd_setup_device(usbd_msg_cb_t msg_cb);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_ZEPHYR_CORE_USB_COMMON_COMMON_USBD_H */
