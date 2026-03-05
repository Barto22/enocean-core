/**
 * @file Ble.cpp
 * @brief POSIX/BlueZ BLE passive scanner implementation.
 *
 * Opens a raw HCI socket, enables LE passive scanning, and dispatches
 * received advertisements to the registered callback on a dedicated thread.
 *
 * HCI event layout for LE Meta Subevent 0x02 (LE Advertising Report):
 *   [0]   HCI packet indicator  (0x04 = HCI_EVENT_PKT)
 *   [1]   Event code            (0x3E = HCI_EV_LE_META)
 *   [2]   Parameter total length
 *   [3]   Subevent code         (0x02 = LE_ADV_REPORT_EVT)
 *   [4]   Num reports
 *   [5]   Event type            (per report)
 *   [6]   Address type          (per report)
 *   [7..12] Address             (per report, LE)
 *   [13]  Data length           (per report)
 *   [14..14+len-1] AD data
 *   [14+len] RSSI (signed)
 */
#include "Ble.hpp"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <logging/Logger.hpp>

namespace Ble {

namespace {

// HCI packet type
constexpr std::uint8_t k_hci_event_pkt{0x04U};
// HCI event codes
constexpr std::uint8_t k_evt_le_meta{0x3EU};
// LE Meta subevent codes
constexpr std::uint8_t k_le_adv_report_evt{0x02U};

// Minimum bytes for one advertising report inside the LE Meta event:
// num_reports(1) + evt_type(1) + addr_type(1) + addr(6) + data_len(1)
// + data(0) + rssi(1) = 11 bytes minimum after the subevent byte.
constexpr std::size_t k_min_adv_report_size{11U};

// Read buffer large enough for maximum HCI event (255 bytes params + header).
constexpr std::size_t k_hci_buf_size{260U};

/// Returns a thread-safe description of the system error @p err.
auto errno_to_str(int err) noexcept -> const char* {
    thread_local std::array<char, 128> buf{};
#if defined(_GNU_SOURCE)
    // GNU strerror_r may return a pointer to a static string instead of buf.
    return strerror_r(err, buf.data(), buf.size());
#else
    (void)strerror_r(err, buf.data(), buf.size());
    return buf.data();
#endif
}

}  // namespace

Scanner::~Scanner() noexcept {
    if (running_.load(std::memory_order_acquire)) {
        running_.store(false, std::memory_order_release);
        disable_le_scan();
    }
    if (hci_fd_ >= 0) {
        ::close(hci_fd_);
        hci_fd_ = -1;
    }
}

auto Scanner::init_impl(AdvCallback cb, void* ctx) noexcept
    -> ErrorHandler<std::monostate, BleError> {
    callback_ = cb;
    callback_ctx_ = ctx;

    dev_id_ = hci_get_route(nullptr);
    if (dev_id_ < 0) {
        return ErrorHandler<std::monostate, BleError>(
            BleError::AdapterNotFound, "No BT adapter found (hci_get_route)");
    }

    hci_fd_ = open_hci_socket();
    if (hci_fd_ < 0) {
        return ErrorHandler<std::monostate, BleError>(
            BleError::SystemError, "Failed to open HCI socket");
    }

    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

auto Scanner::start_scan_impl() noexcept
    -> ErrorHandler<std::monostate, BleError> {
    if (hci_fd_ < 0) {
        return ErrorHandler<std::monostate, BleError>(
            BleError::NotInitialised, "Call init() before start_scan()");
    }
    if (running_.load(std::memory_order_acquire)) {
        return ErrorHandler<std::monostate, BleError>(std::monostate{});
    }

    if (!enable_le_scan()) {
        return ErrorHandler<std::monostate, BleError>(
            BleError::ScanStartFailed, "hci_le_set_scan_enable failed");
    }

    running_.store(true, std::memory_order_release);

    constexpr PlatformCore::Thread::ThreadStackInfo stack{
        .stack_size = 8192U, .stack_pointer = nullptr};
    PlatformCore::ThreadParamVariant params{PlatformCore::ThreadParam1{
        .param = PlatformCore::ThreadParamFromPointer(this)}};
    const auto result{scan_thread_.create(scan_thread_fn, stack, 5, params)};
    if (!result) {
        running_.store(false, std::memory_order_release);
        disable_le_scan();
        return ErrorHandler<std::monostate, BleError>(
            BleError::SystemError, "Failed to create scan thread");
    }
    LOGGER_NOTICE("BLE passive scan started (hci%d)", dev_id_);
    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

auto Scanner::stop_scan_impl() noexcept
    -> ErrorHandler<std::monostate, BleError> {
    if (!running_.load(std::memory_order_acquire)) {
        return ErrorHandler<std::monostate, BleError>(std::monostate{});
    }
    running_.store(false, std::memory_order_release);
    disable_le_scan();
    LOGGER_NOTICE("BLE passive scan stopped");
    return ErrorHandler<std::monostate, BleError>(std::monostate{});
}

int Scanner::open_hci_socket() noexcept {
    const int fd{hci_open_dev(dev_id_)};
    if (fd < 0) {
        const int saved_errno{errno};
        LOGGER_ERROR("hci_open_dev(%d) failed: %s", dev_id_,
                     errno_to_str(saved_errno));
        return -1;
    }

    struct hci_filter flt{};
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_LE_META_EVENT, &flt);

    if (::setsockopt(fd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        const int saved_errno{errno};
        LOGGER_ERROR("setsockopt(HCI_FILTER) failed: %s",
                     errno_to_str(saved_errno));
        ::close(fd);
        return -1;
    }
    return fd;
}

bool Scanner::enable_le_scan() noexcept {
    // Passive scan, 100 ms interval, 100 ms window, own address = public,
    // no duplicate filtering (show all so we don't miss any device).
    constexpr std::uint16_t k_scan_interval{0x00A0U};  // 100 ms
    constexpr std::uint16_t k_scan_window{0x00A0U};    // 100 ms
    constexpr int k_scan_timeout_ms{1000};

    // Disable scanning first — the controller rejects set_scan_parameters
    // with EIO if a scan is already active (e.g. started by bluetoothd).
    (void)hci_le_set_scan_enable(hci_fd_, 0x00U, 0x00U, k_scan_timeout_ms);

    if (hci_le_set_scan_parameters(hci_fd_,
                                   0x00U,  // passive
                                   k_scan_interval, k_scan_window,
                                   0x00U,  // own addr: public
                                   0x00U,  // no whitelist
                                   k_scan_timeout_ms) < 0) {
        const int saved_errno{errno};
        LOGGER_ERROR("hci_le_set_scan_parameters failed: %s",
                     errno_to_str(saved_errno));
        return false;
    }

    if (hci_le_set_scan_enable(hci_fd_, 0x01U, 0x00U, k_scan_timeout_ms) < 0) {
        const int saved_errno{errno};
        LOGGER_ERROR("hci_le_set_scan_enable failed: %s",
                     errno_to_str(saved_errno));
        return false;
    }
    return true;
}

void Scanner::disable_le_scan() noexcept {
    if (hci_fd_ >= 0) {
        (void)hci_le_set_scan_enable(hci_fd_, 0x00U, 0x00U, 1000);
    }
}

auto Scanner::scan_thread_fn(void* ctx) -> void* {
    if (ctx == nullptr) {
        return nullptr;
    }
    static_cast<Scanner*>(ctx)->scan_loop();
    return nullptr;
}

void Scanner::scan_loop() noexcept {
    std::array<std::uint8_t, k_hci_buf_size> buf{};

    while (running_.load(std::memory_order_acquire)) {
        const ssize_t len{::read(hci_fd_, buf.data(), buf.size())};
        if (len <= 0) {
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            continue;
        }

        const auto total{static_cast<std::size_t>(len)};

        // 5-byte HCI/LE-meta header + at least one full advertising report.
        if (total < 5U + k_min_adv_report_size) {
            continue;
        }
        if (buf[0U] != k_hci_event_pkt) {
            continue;
        }
        if (buf[1U] != k_evt_le_meta) {
            continue;
        }
        if (buf[3U] != k_le_adv_report_evt) {
            continue;
        }

        const std::uint8_t num_reports{buf[4U]};
        if (num_reports == 0U) {
            continue;
        }

        dispatch_adv_report(buf.data() + 5U, total - 5U, num_reports);
    }
}

void Scanner::dispatch_adv_report(const std::uint8_t* report,
                                  std::size_t report_buf_len,
                                  std::uint8_t num_reports) noexcept {
    if ((report == nullptr) || (callback_ == nullptr)) {
        return;
    }

    // Sanity cap: the LE spec allows at most a handful of reports per event.
    constexpr std::uint8_t k_max_reports{15U};
    if (num_reports > k_max_reports) {
        return;
    }

    const std::uint8_t* const buf_end{report + report_buf_len};
    const std::uint8_t* ptr{report};

    for (std::uint8_t r{0U}; r < num_reports; ++r) {
        // Minimum record: type(1)+addr_type(1)+addr(6)+data_len(1) = 9 bytes,
        // plus at least rssi(1) = 10 bytes total before reading data_len.
        constexpr std::size_t k_report_hdr{10U};
        if (ptr + k_report_hdr > buf_end) {
            break;
        }

        const std::uint8_t adv_type{ptr[0U]};
        const std::uint8_t addr_type{ptr[1U]};
        const std::uint8_t data_len{ptr[8U]};

        // Validate that data + rssi byte fit within the received buffer.
        if (ptr + k_report_hdr + static_cast<std::size_t>(data_len) > buf_end) {
            break;
        }

        const std::uint8_t* addr_ptr{ptr + 2U};
        const std::uint8_t* data_ptr{ptr + 9U};
        const auto* rssi_ptr{data_ptr + data_len};

        const auto rssi{static_cast<std::int8_t>(*rssi_ptr)};

        callback_(callback_ctx_,
                  std::span<const std::uint8_t>(addr_ptr,
                                                static_cast<std::size_t>(6U)),
                  addr_type, adv_type, rssi,
                  std::span<const std::uint8_t>(
                      data_ptr, static_cast<std::size_t>(data_len)));

        ptr = rssi_ptr + 1U;
    }
}

}  // namespace Ble
