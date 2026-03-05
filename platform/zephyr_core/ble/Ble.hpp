/**
 * @file Ble.hpp
 * @brief Zephyr implementation of the BLE passive scanner interface.
 *
 * Wraps the Zephyr Bluetooth observer (bt_le_scan_cb_register) and
 * forwards each advertisement to the registered AdvCallback.
 *
 * The application must call bt_enable(NULL) before calling init().
 */
#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include <atomic>
#include <cstdint>
#include <span>

#include "IBle.hpp"

namespace Ble {

/**
 * @brief Zephyr Bluetooth passive BLE scanner.
 *
 * Implements IBle<Scanner> for the Zephyr RTOS platform.
 *
 * The Zephyr BT stack calls the observer callback from the BT RX work-queue
 * context.  Applications must ensure that the registered AdvCallback is
 * thread-safe.
 *
 * Only one Scanner instance should exist at a time because
 * bt_le_scan_cb_register accepts a single scan callback.
 */
class Scanner final : public IBle<Scanner> {
   public:
    Scanner() noexcept = default;
    ~Scanner() noexcept = default;

    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    Scanner(Scanner&&) = delete;
    Scanner& operator=(Scanner&&) = delete;

    /**
     * @brief Initialize the Zephyr BLE scanner and register callback.
     *
     * Registers the advertisement callback and stores the context pointer.
     * Also registers the internal Zephyr scan callback with
     * bt_le_scan_cb_register.
     *
     * @param cb Advertisement callback function (must not be nullptr)
     * @param ctx User context pointer passed to callback (may be nullptr)
     * @return ErrorHandler with success (std::monostate) or BleError
     */
    [[nodiscard]] auto init_impl(AdvCallback cb, void* ctx) noexcept
        -> ErrorHandler<std::monostate, BleError>;

    /**
     * @brief Start passive BLE scanning.
     *
     * Configures and starts BLE passive scanning using bt_le_scan_start with
     * Zephyr's passive scan parameters. Must be called after init_impl().
     *
     * @return ErrorHandler with success (std::monostate) or BleError
     */
    [[nodiscard]] auto start_scan_impl() noexcept
        -> ErrorHandler<std::monostate, BleError>;

    /**
     * @brief Stop BLE scanning.
     *
     * Stops the active BLE scan using bt_le_scan_stop.
     *
     * @return ErrorHandler with success (std::monostate) or BleError
     */
    [[nodiscard]] auto stop_scan_impl() noexcept
        -> ErrorHandler<std::monostate, BleError>;

   private:
    /// Trampoline: Zephyr calls this via bt_le_scan_cb_register.
    /// New Zephyr API (≥3.x): recv receives a bt_le_scan_recv_info struct.
    static void zephyr_scan_cb(const struct bt_le_scan_recv_info* info,
                               struct net_buf_simple* buf) noexcept;

    AdvCallback callback_{nullptr};
    void* callback_ctx_{nullptr};

    /// Zephyr scan callback struct registered with bt_le_scan_cb_register.
    bt_le_scan_cb scan_cb_{
        .recv = zephyr_scan_cb,
    };

    /// Pointer to the single active scanner instance used by the static cb.
    /// Atomic because init_impl() writes it from the application context while
    /// zephyr_scan_cb() reads it from the BT RX work-queue context.
    static std::atomic<Scanner*> s_instance_;
};

static_assert(Ble::BleImplementation<Scanner>,
              "Scanner must satisfy Ble::BleImplementation concept");

}  // namespace Ble
