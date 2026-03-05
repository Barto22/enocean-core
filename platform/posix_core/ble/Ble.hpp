/**
 * @file Ble.hpp
 * @brief POSIX/BlueZ implementation of the BLE passive scanner interface.
 *
 * Uses raw HCI sockets (AF_BLUETOOTH / BTPROTO_HCI) to receive LE
 * advertising events from the kernel BlueZ stack.  Scanning runs on
 * a dedicated PlatformCore::Thread so the calling thread is not blocked.
 *
 * Requires:
 *   - libbluetooth-dev  (apt install libbluetooth-dev)
 *   - CAP_NET_RAW or root privileges at runtime
 *
 * Note: Only the first HCI device (hci0) is used.  This can be made
 * configurable in a future extension via the init() overload.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <span>

#include "IBle.hpp"
#include "Thread.hpp"

namespace Ble {

/**
 * @brief BlueZ HCI BLE passive scanner.
 *
 * Implements IBle<Scanner> for POSIX platforms.
 *
 * Thread safety:
 *   The internal scanning thread calls the registered advertisement callback
 *   from its own context.  The application must ensure that the callback
 *   implementation (e.g., EnoceanDriver::process_advertisement) is
 *   thread-safe or that scans are serialised externally.
 */
class Scanner final : public IBle<Scanner> {
   public:
    Scanner() noexcept = default;
    ~Scanner() noexcept;

    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    Scanner(Scanner&&) = delete;
    Scanner& operator=(Scanner&&) = delete;

    /**
     * @brief Initialize the POSIX HCI BLE scanner and register callback.
     *
     * Opens and configures the HCI socket for BLE scanning. Registers the
     * advertisement callback and stores the context pointer. Requires
     * CAP_NET_RAW capability or root privileges.
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
     * Enables LE passive scanning on the HCI device and spawns a dedicated
     * thread to receive and dispatch advertising reports. Must be called
     * after init_impl().
     *
     * @return ErrorHandler with success (std::monostate) or BleError
     */
    [[nodiscard]] auto start_scan_impl() noexcept
        -> ErrorHandler<std::monostate, BleError>;

    /**
     * @brief Stop BLE scanning.
     *
     * Disables LE scanning on the HCI device and signals the scan thread
     * to terminate.
     *
     * @return ErrorHandler with success (std::monostate) or BleError
     */
    [[nodiscard]] auto stop_scan_impl() noexcept
        -> ErrorHandler<std::monostate, BleError>;

   private:
    /// Open and configure the HCI socket.  Returns the fd or -1 on error.
    [[nodiscard]] int open_hci_socket() noexcept;

    /// Enable LE passive scanning on the open socket.
    [[nodiscard]] bool enable_le_scan() noexcept;

    /// Disable LE scanning.
    void disable_le_scan() noexcept;

    /// Scanning thread entry (C-linkage trampoline calls this via CRTP).
    static auto scan_thread_fn(void* ctx) -> void*;

    /// Core scanning loop executed by scan_thread_fn.
    void scan_loop() noexcept;

    /// Parse LE advertising reports and invoke the callback.
    /// @param report     Pointer to the start of the first report record.
    /// @param report_buf_len  Number of valid bytes from @p report onward.
    /// @param num_reports Number of reports claimed by the HCI event.
    void dispatch_adv_report(const std::uint8_t* report,
                             std::size_t report_buf_len,
                             std::uint8_t num_reports) noexcept;

    AdvCallback callback_{nullptr};
    void* callback_ctx_{nullptr};
    int hci_fd_{-1};  ///< HCI socket file descriptor.
    int dev_id_{-1};  ///< HCI device index (e.g. 0).
    std::atomic<bool> running_{false};
    PlatformCore::Thread scan_thread_{"BleScanner"};
};

static_assert(Ble::BleImplementation<Scanner>,
              "Scanner must satisfy Ble::BleImplementation concept");

}  // namespace Ble
