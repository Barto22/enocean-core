/**
 * @file Ble.cpp
 * @brief Zephyr BLE passive scanner implementation.
 */
#include "Ble.hpp"

#include <zephyr/bluetooth/bluetooth.h>

#include <logging/Logger.hpp>

namespace Ble {

std::atomic<Scanner*> Scanner::s_instance_{nullptr};

auto Scanner::init_impl(AdvCallback cb, void* ctx) noexcept
    -> ErrorHandler<std::monostate, BleError> {
    callback_ = cb;
    callback_ctx_ = ctx;
    s_instance_.store(this, std::memory_order_release);
    bt_le_scan_cb_register(&scan_cb_);
    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

auto Scanner::start_scan_impl() noexcept
    -> ErrorHandler<std::monostate, BleError> {
    // BT_LE_SCAN_PASSIVE is a C compound-literal macro (temporary array
    // pointer) which is invalid in C++.  Use BT_LE_SCAN_PARAM_INIT to
    // initialise a named variable and pass its address instead.
    struct bt_le_scan_param scan_params{
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };
    const int err{bt_le_scan_start(&scan_params, nullptr)};
    if (err != 0) {
        LOGGER_ERROR("bt_le_scan_start failed (%d)", err);
        return ErrorHandler<std::monostate, BleError>(
            BleError::ScanStartFailed, "bt_le_scan_start failed");
    }

    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

auto Scanner::stop_scan_impl() noexcept
    -> ErrorHandler<std::monostate, BleError> {
    const int err{bt_le_scan_stop()};
    if (err != 0) {
        LOGGER_ERROR("bt_le_scan_stop failed (%d)", err);
        return ErrorHandler<std::monostate, BleError>(BleError::ScanStopFailed,
                                                      "bt_le_scan_stop failed");
    }
    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

void Scanner::zephyr_scan_cb(const struct bt_le_scan_recv_info* info,
                             struct net_buf_simple* buf) noexcept {
    Scanner* const inst{s_instance_.load(std::memory_order_acquire)};
    if ((inst == nullptr) || (inst->callback_ == nullptr) ||
        (info == nullptr) || (info->addr == nullptr) || (buf == nullptr)) {
        return;
    }

    inst->callback_(
        inst->callback_ctx_,
        std::span<const std::uint8_t>(info->addr->a.val,
                                      static_cast<std::size_t>(BT_ADDR_SIZE)),
        info->addr->type, info->adv_type, info->rssi,
        std::span<const std::uint8_t>(buf->data,
                                      static_cast<std::size_t>(buf->len)));
}

}  // namespace Ble
