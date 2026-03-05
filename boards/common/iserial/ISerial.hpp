
/**
 * @file ISerial.hpp
 * @brief Serial abstraction interface and concept for cross-platform hardware
 * drivers.
 *
 * This file defines the generic serial interface (`SerialDriverInterface`) and
 * concept for validating platform-specific serial implementations. The
 * interface provides a modern C++ abstraction for serial communication,
 * supporting multiple platforms and driver types.
 *
 * - `SerialDriverInterface` is an abstract base class for serial drivers.
 * - `SerialDriverConcept` concept checks for valid serial implementations.
 * - `Serial` is a type-safe wrapper for serial drivers.
 *
 * Usage:
 *   - Implement `SerialDriverInterface` for your platform (e.g., STM32, POSIX,
 * Zephyr).
 *   - Use the interface for portable serial operations in your application.
 *   - Use the `Serial` wrapper for RAII and type-safe access.
 */
#pragma once

#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "SerialError.hpp"

/**
 * @namespace Boards::Serial
 * @brief Namespace for common serial abstractions and concepts.
 */
namespace Boards::Serial {

template <typename T>
concept SerialDriverImplementation =
    requires(T device, const std::uint8_t* tx_ptr, std::uint8_t* rx_ptr,
             std::size_t len, std::uint32_t timeout) {
        {
            device.init_impl()
        } -> std::same_as<ErrorHandler<std::monostate, SerialError>>;
        { device.deinit_impl() } -> std::same_as<void>;
        {
            device.poll_write_impl(tx_ptr, len, timeout)
        } -> std::same_as<std::size_t>;
        {
            device.poll_read_impl(rx_ptr, len, timeout)
        } -> std::same_as<std::size_t>;
        {
            device.non_blocking_write_impl(tx_ptr, len, timeout)
        } -> std::same_as<std::size_t>;
        {
            device.non_blocking_read_impl(rx_ptr, len, timeout)
        } -> std::same_as<std::size_t>;
        {
            std::as_const(device).get_tx_pending_impl()
        } -> std::same_as<std::size_t>;
        {
            std::as_const(device).get_rx_pending_impl()
        } -> std::same_as<std::size_t>;
        { device.flush_tx_impl() } -> std::same_as<void>;
        { device.flush_rx_impl() } -> std::same_as<void>;
    };

/**
 * @class SerialDriverInterface
 * @brief Template base class for generic serial drivers using CRTP.
 *
 * Provides a platform-agnostic interface for serial communication via static
 * polymorphism.
 * @tparam Derived The specific implementation class.
 */
template <typename Derived>
class SerialDriverInterface {
   public:
    /**
     * @brief Default constructor.
     */
    consteval SerialDriverInterface() noexcept = default;

    /**
     * @brief Deleted copy constructor.
     */
    SerialDriverInterface(const SerialDriverInterface&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    SerialDriverInterface& operator=(const SerialDriverInterface&) = delete;

    /**
     * @brief Default move constructor.
     */
    SerialDriverInterface(SerialDriverInterface&&) noexcept = default;

    /**
     * @brief Default move assignment operator.
     */
    SerialDriverInterface& operator=(SerialDriverInterface&&) noexcept =
        default;

    /**
     * @brief Initialize the serial driver.
     * @return ErrorHandler with std::monostate on success, SerialError on
     * failure
     */
    [[nodiscard]] auto init() -> ErrorHandler<std::monostate, SerialError> {
        return static_cast<Derived*>(this)->init_impl();
    }

    /**
     * @brief Deinitialize the serial driver.
     */
    void deinit() { static_cast<Derived*>(this)->deinit_impl(); }

    /**
     * @brief Write data to the serial port in polling mode.
     * @param tx_buf Transmit buffer pointer.
     * @param tx_len Number of bytes to transmit.
     * @param timeout_ms Timeout in milliseconds.
     * @return Number of bytes written.
     */
    auto poll_write(const std::uint8_t* tx_buf, size_t tx_len,
                    std::uint32_t timeout_ms) -> size_t {
        return static_cast<Derived*>(this)->poll_write_impl(tx_buf, tx_len,
                                                            timeout_ms);
    }

    /**
     * @brief Read data from the serial port in polling mode.
     * @param rx_buf Receive buffer pointer.
     * @param rx_len Number of bytes to receive.
     * @param timeout_ms Timeout in milliseconds.
     * @return Number of bytes read.
     */
    auto poll_read(std::uint8_t* rx_buf, size_t rx_len,
                   std::uint32_t timeout_ms) -> size_t {
        return static_cast<Derived*>(this)->poll_read_impl(rx_buf, rx_len,
                                                           timeout_ms);
    }

    /**
     * @brief Write data to the serial port in non-blocking mode.
     * @param tx_buf Transmit buffer pointer.
     * @param tx_len Number of bytes to transmit.
     * @param timeout_ms Timeout in milliseconds.
     * @return Number of bytes written.
     */
    auto non_blocking_write(const std::uint8_t* tx_buf, size_t tx_len,
                            std::uint32_t timeout_ms) -> size_t {
        return static_cast<Derived*>(this)->non_blocking_write_impl(
            tx_buf, tx_len, timeout_ms);
    }

    /**
     * @brief Read data from the serial port in non-blocking mode.
     * @param rx_buf Receive buffer pointer.
     * @param rx_len Number of bytes to receive.
     * @param timeout_ms Timeout in milliseconds.
     * @return Number of bytes read.
     */
    auto non_blocking_read(std::uint8_t* rx_buf, size_t rx_len,
                           std::uint32_t timeout_ms) -> size_t {
        return static_cast<Derived*>(this)->non_blocking_read_impl(
            rx_buf, rx_len, timeout_ms);
    }

    /**
     * @brief Get the number of bytes pending in the transmit buffer.
     * @return Number of bytes pending for transmission.
     */
    auto get_tx_pending() const -> size_t {
        return static_cast<const Derived*>(this)->get_tx_pending_impl();
    }

    /**
     * @brief Get the number of bytes pending in the receive buffer.
     * @return Number of bytes pending for reception.
     */
    auto get_rx_pending() const -> size_t {
        return static_cast<const Derived*>(this)->get_rx_pending_impl();
    }

    /**
     * @brief Flush the transmit buffer.
     */
    void flush_tx() { static_cast<Derived*>(this)->flush_tx_impl(); }

    /**
     * @brief Flush the receive buffer.
     */
    void flush_rx() { static_cast<Derived*>(this)->flush_rx_impl(); }

   protected:
    /**
     * @brief Protected destructor to prevent deletion via base pointer.
     */
    ~SerialDriverInterface() = default;
};

/**
 * @brief Concept for valid serial driver implementations.
 * @tparam T Serial driver type.
 */
template <typename T>
concept SerialDriverConcept = requires(T driver, const std::uint8_t* tx_buf,
                                       std::uint8_t* rx_buf, size_t len,
                                       std::uint32_t timeout) {
    {
        driver.init()
    }
    -> std::same_as<ErrorHandler<std::monostate, Boards::Serial::SerialError>>;
    { driver.deinit() } -> std::same_as<void>;

    { driver.poll_write(tx_buf, len, timeout) } -> std::same_as<size_t>;
    { driver.poll_read(rx_buf, len, timeout) } -> std::same_as<size_t>;

    { driver.non_blocking_write(tx_buf, len, timeout) } -> std::same_as<size_t>;
    { driver.non_blocking_read(rx_buf, len, timeout) } -> std::same_as<size_t>;

    { std::as_const(driver).get_tx_pending() } -> std::same_as<size_t>;
    { std::as_const(driver).get_rx_pending() } -> std::same_as<size_t>;

    { driver.flush_tx() } -> std::same_as<void>;
    { driver.flush_rx() } -> std::same_as<void>;
};

/**
 * @class Serial
 * @brief Type-safe wrapper for serial drivers.
 *
 * Provides RAII and type-safe access to serial driver functionality.
 *
 * @tparam DriverT Serial driver type satisfying SerialDriverConcept.
 */
template <SerialDriverConcept DriverT>
class Serial {
   public:
    /**
     * @brief Construct a Serial wrapper with a driver instance.
     * @param driver Serial driver instance.
     */
    explicit Serial(DriverT&& driver) noexcept : driver_(std::move(driver)) {}

    /**
     * @brief Destructor. Deinitializes the driver if needed.
     */
    ~Serial() noexcept { cleanup(); }

    /**
     * @brief Initialize the serial driver if not already initialized.
     * @return True if initialization succeeded, false otherwise.
     */
    auto initialize() -> bool {
        if (!is_initialized_) {
            const auto result{driver_.init()};
            is_initialized_ = result.has_value();
        }
        return is_initialized_;
    }

    /**
     * @brief Deleted copy constructor.
     */
    Serial(const Serial&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    Serial& operator=(const Serial&) = delete;

    /**
     * @brief Move constructor.
     */
    Serial(Serial&& other) noexcept
        : driver_(std::move(other.driver_)),
          is_initialized_(std::exchange(other.is_initialized_, false)) {}

    /**
     * @brief Move assignment operator.
     */
    Serial& operator=(Serial&& other) noexcept {
        if (this != &other) {
            cleanup();
            driver_ = std::move(other.driver_);
            is_initialized_ = std::exchange(other.is_initialized_, false);
        }
        return *this;
    }

    /**
     * @brief Write data to the serial port in non-blocking mode.
     * @param data Data buffer to write.
     * @param timeout Timeout in milliseconds.
     * @return Number of bytes written.
     */
    size_t write(std::span<const std::uint8_t> data, std::uint32_t timeout) {
        if (!is_initialized_ || data.empty()) return 0;
        return driver_.non_blocking_write(data.data(), data.size(), timeout);
    }

    /**
     * @brief Read data from the serial port in non-blocking mode.
     * @param buffer Buffer to read into.
     * @param timeout Timeout in milliseconds.
     * @return Number of bytes read.
     */
    size_t read(std::span<std::uint8_t> buffer, std::uint32_t timeout) {
        if (!is_initialized_ || buffer.empty()) return 0;
        return driver_.non_blocking_read(buffer.data(), buffer.size(), timeout);
    }

    /** @brief Get the number of bytes pending for transmission. */
    size_t get_tx_pending() const { return driver_.get_tx_pending(); }
    /** @brief Get the number of bytes pending for reception. */
    size_t get_rx_pending() const { return driver_.get_rx_pending(); }
    /** @brief Flush the transmit buffer. */
    void flush_tx() { driver_.flush_tx(); }
    /** @brief Flush the receive buffer. */
    void flush_rx() { driver_.flush_rx(); }

    /** @brief Get const reference to the underlying driver. */
    DriverT& get_driver() noexcept { return driver_; }

   private:
    /** @brief Serial driver instance. */
    DriverT driver_{};
    /** @brief Initialization state. */
    bool is_initialized_{false};
    /** @brief Cleanup resources. */
    void cleanup() noexcept {
        if (is_initialized_) {
            driver_.deinit();
            is_initialized_ = false;
        }
    }
};

}  // namespace Boards::Serial