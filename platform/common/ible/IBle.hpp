/**
 * @file IBle.hpp
 * @brief CRTP interface for platform-specific BLE passive scanners.
 *
 * Provides a compile-time polymorphic contract for BLE scanning, consistent
 * with the IThread / IMessageQueue pattern used throughout the project.
 *
 * Platforms implement this interface by deriving from IBle<Derived> and
 * providing the three _impl methods listed in the BleImplementation concept.
 *
 * Advertisement callback signature:
 * @code
 *   void my_cb(void*                         ctx,
 *              std::span<const std::uint8_t> addr,        // 6 bytes, LE
 *              std::uint8_t                  addr_type,
 *              std::uint8_t                  adv_type,    // HCI evt type
 *              std::int8_t                   rssi,
 *              std::span<const std::uint8_t> adv_data);
 * @endcode
 *
 * Usage:
 * @code
 *   Ble::Scanner scanner{};
 *   scanner.init(my_cb, &my_driver);
 *   scanner.start_scan();
 * @endcode
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>

#include "BleError.hpp"
#include "error_handler/ErrorHandler.hpp"

namespace Ble {

/**
 * @brief Advertisement received callback type.
 *
 * @param ctx       User-provided context pointer (may be nullptr if unused).
 * @param addr      6-byte BLE address in LE wire order.
 * @param addr_type 0 = public, 1 = random.
 * @param adv_type  HCI advertisement event type (0x00..0x04).
 * @param rssi      Received signal strength in dBm.
 * @param adv_data  Raw advertisement payload (max 31 bytes).
 */
using AdvCallback = void (*)(void* ctx, std::span<const std::uint8_t> addr,
                             std::uint8_t addr_type, std::uint8_t adv_type,
                             std::int8_t rssi,
                             std::span<const std::uint8_t> adv_data) noexcept;

template <typename D>
concept BleImplementation = requires(D d, AdvCallback cb, void* ctx) {
    {
        d.init_impl(cb, ctx)
    } -> std::same_as<ErrorHandler<std::monostate, BleError>>;
    {
        d.start_scan_impl()
    } -> std::same_as<ErrorHandler<std::monostate, BleError>>;
    {
        d.stop_scan_impl()
    } -> std::same_as<ErrorHandler<std::monostate, BleError>>;
    requires !std::is_abstract_v<D>;
};

/**
 * @brief CRTP base class for platform-specific BLE passive scanners.
 *
 * @tparam Derived  The concrete platform implementation
 *                  (e.g., Ble::Scanner in posix_core or zephyr_core).
 */
template <typename Derived>
class IBle {
   public:
    IBle() = default;
    IBle(const IBle&) = delete;
    IBle& operator=(const IBle&) = delete;
    IBle(IBle&&) = delete;
    IBle& operator=(IBle&&) = delete;

   protected:
    ~IBle() noexcept = default;

   public:
    /**
     * @brief Initialise the BLE adapter and register the advertisement
     * callback.
     *
     * Must be called before start_scan().  The @p ctx pointer is forwarded
     * as-is to every invocation of @p cb.
     *
     * @param cb   Advertisement callback (must not be nullptr).
     * @param ctx  User context passed to cb on every advertisement.
     * @return Success or BleError.
     */
    [[nodiscard]] auto init(AdvCallback cb, void* ctx) noexcept
        -> ErrorHandler<std::monostate, BleError> {
        if (cb == nullptr) {
            return ErrorHandler<std::monostate, BleError>(
                BleError::InvalidCallback, "Advertisement callback is null");
        }
        return static_cast<Derived*>(this)->init_impl(cb, ctx);
    }

    /**
     * @brief Start passive BLE scanning.
     *
     * @return Success or BleError.
     */
    [[nodiscard]] auto start_scan() noexcept
        -> ErrorHandler<std::monostate, BleError> {
        return static_cast<Derived*>(this)->start_scan_impl();
    }

    /**
     * @brief Stop BLE scanning.
     *
     * @return Success or BleError.
     */
    [[nodiscard]] auto stop_scan() noexcept
        -> ErrorHandler<std::monostate, BleError> {
        return static_cast<Derived*>(this)->stop_scan_impl();
    }
};

}  // namespace Ble
