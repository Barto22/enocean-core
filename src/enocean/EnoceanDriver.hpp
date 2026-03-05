/**
 * @file EnoceanDriver.hpp
 * @brief Platform-agnostic EnOcean BLE driver.
 *
 * EnoceanDriver is the core, platform-independent library that:
 *   1. Accepts raw BLE advertisement packets via process_advertisement().
 *   2. Identifies EnOcean manufacturer-specific data (Company ID 0x03DA).
 *   3. Determines the device type (switch / sensor) from the BLE address.
 *   4. For unknown devices in commissioning mode: extracts the AES key and
 *      registers the device (commissioning flow).
 *   5. For known devices: verifies the AES-CCM-4 authentication tag and
 *      dispatches the decoded payload via the registered callbacks.
 *   6. Enforces sequence-number monotonicity to prevent replay attacks.
 *
 * The driver is templated on a CryptoBackend type (ICrypto<Derived>) so
 * the AES-CCM operation is resolved at compile time with zero virtual-
 * function overhead, consistent with the rest of this codebase.
 *
 * Usage (POSIX example):
 * @code
 *   Crypto::AesCcm  crypto{};
 *   Enocean::EnoceanDriver<Crypto::AesCcm> driver{crypto, callbacks};
 *   driver.init();
 *   driver.enable_commissioning();
 *   // ... BLE scanner calls driver.process_advertisement(pkt) ...
 * @endcode
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

#include "EnoceanError.hpp"
#include "EnoceanTypes.hpp"
#include "error_handler/ErrorHandler.hpp"

namespace Enocean {

/**
 * @brief Platform-agnostic EnOcean BLE advertisement driver.
 *
 * @tparam CryptoBackend  Concrete implementation of ICrypto<CryptoBackend>
 *                        (e.g., Crypto::AesCcm from posix_core or zephyr_core).
 */
template <typename CryptoBackend>
class EnoceanDriver final {
   public:
    /**
     * @brief Construct the driver.
     *
     * @param crypto    Reference to the platform-specific AES-CCM backend.
     * @param callbacks Application callback table (any entry may be nullptr).
     *
     * @note Does not perform any BLE or hardware operations.  Call init()
     *       before passing packets.
     */
    explicit EnoceanDriver(CryptoBackend& crypto,
                           const EnoceanCallbacks& callbacks) noexcept
        : crypto_{crypto}, callbacks_{callbacks} {}

    EnoceanDriver(const EnoceanDriver&) = delete;
    EnoceanDriver& operator=(const EnoceanDriver&) = delete;
    EnoceanDriver(EnoceanDriver&&) = delete;
    EnoceanDriver& operator=(EnoceanDriver&&) = delete;

    ~EnoceanDriver() noexcept = default;

    /**
     * @brief Initialise the driver and reset device table.
     *
     * Must be called once before process_advertisement().
     *
     * @return Success or EnoceanError on failure.
     */
    [[nodiscard]] auto init() noexcept
        -> ErrorHandler<std::monostate, EnoceanError>;

    /**
     * @brief Enable commissioning mode.
     *
     * While enabled, the driver will accept commissioning advertisements
     * from previously unknown EnOcean devices and add them to the table.
     */
    void enable_commissioning() noexcept;

    /**
     * @brief Disable commissioning mode.
     *
     * New devices are silently ignored; only already-commissioned devices
     * can send authenticated events.
     */
    void disable_commissioning() noexcept;

    /**
     * @brief Query whether commissioning mode is currently active.
     * @return true if commissioning is enabled.
     */
    [[nodiscard]] bool commissioning_enabled() const noexcept;

    /**
     * @brief BLE advertisement callback entry point.
     *
     * Intended to be called from the BLE scanning thread for every
     * received non-connectable advertisement.  Thread-safety is the
     * caller's responsibility (the BLE layer must serialise calls or
     * acquire a lock).
     *
     * @param addr      6-byte BLE address (LE byte order).
     * @param addr_type Address type: 0 = public, 1 = random.
     * @param adv_type  HCI advertisement event type (0x00 = ADV_IND, etc.).
     * @param rssi      Received signal strength in dBm.
     * @param data      Raw advertisement payload (max 31 bytes).
     */
    void process_advertisement(std::span<const std::uint8_t> addr,
                               std::uint8_t addr_type, std::uint8_t adv_type,
                               std::int8_t rssi,
                               std::span<const std::uint8_t> data) noexcept;

    /**
     * @brief Static trampoline for use as a C-style callback.
     *
     * Casts @p ctx to EnoceanDriver* and forwards to process_advertisement().
     * Suitable for platforms that require a plain function pointer with a
     * void* context (e.g., the IBle::init(cb, ctx) overload).
     */
    static void advertisement_cb(void* ctx, std::span<const std::uint8_t> addr,
                                 std::uint8_t addr_type, std::uint8_t adv_type,
                                 std::int8_t rssi,
                                 std::span<const std::uint8_t> data) noexcept;

    /**
     * @brief Return the number of currently commissioned devices.
     * @return Count in [0, k_max_devices].
     */
    [[nodiscard]] std::size_t device_count() const noexcept;

   private:
    /// Locate a device slot by BLE address.  Returns nullptr if not found.
    [[nodiscard]] EnoceanDevice* find_device(
        std::span<const std::uint8_t> addr) noexcept;

    /// Allocate a new device slot.  Returns nullptr if table is full.
    [[nodiscard]] EnoceanDevice* allocate_device() noexcept;

    /// Determine the device type from the BLE address bytes.
    [[nodiscard]] static DeviceType detect_device_type(
        std::span<const std::uint8_t> addr) noexcept;

    /**
     * @brief Extract the EnOcean manufacturer-specific AD structure.
     *
     * Walks the AD type-length-value list looking for AD type 0xFF with
     * Company ID 0x03DA.  Returns a span over the payload bytes following
     * the 2-byte Company ID, or an empty span on failure.
     *
     * @param adv_data  Raw advertisement payload.
     * @return Span over the manufacturer payload (excluding Company ID bytes).
     */
    [[nodiscard]] static std::span<const std::uint8_t>
    extract_manufacturer_data(std::span<const std::uint8_t> adv_data) noexcept;

    /**
     * @brief Handle a commissioning advertisement.
     *
     * Expects @p mfr_payload to contain:
     *   [0]      payload length (must be ≥ 20)
     *   [1..4]   sequence number (LE uint32)
     *   [5..20]  AES-128 key
     *
     * @param addr        Device BLE address (6 bytes).
     * @param addr_type   BLE address type.
     * @param rssi        RSSI in dBm.
     * @param mfr_payload Manufacturer payload (after Company ID).
     * @return true on successful commissioning.
     */
    bool handle_commissioning(
        std::span<const std::uint8_t> addr, std::uint8_t addr_type,
        std::int8_t rssi, std::span<const std::uint8_t> mfr_payload) noexcept;

    /**
     * @brief Verify AES-CCM-4 authentication and decode a switch event.
     *
     * @param device      Commissioned device entry.
     * @param mfr_payload Manufacturer payload (after Company ID).
     * @return true on successful authentication and dispatch.
     */
    bool handle_switch(EnoceanDevice& device,
                       std::span<const std::uint8_t> mfr_payload) noexcept;

    /**
     * @brief Verify AES-CCM-4 authentication and decode a sensor report.
     *
     * @param device      Commissioned device entry.
     * @param mfr_payload Manufacturer payload (after Company ID).
     * @return true on successful authentication and dispatch.
     */
    bool handle_sensor(EnoceanDevice& device,
                       std::span<const std::uint8_t> mfr_payload) noexcept;

    /**
     * @brief Build a 13-byte CCM nonce from device address and seq number.
     *
     * Nonce layout (as used by EnOcean):
     *   [0..5]  BLE address (LE, addr[0] = LSB)
     *   [6..9]  sequence number (LE uint32)
     *   [10..12] zero padding
     *
     * @param addr   6-byte BLE address.
     * @param seq    Sequence number.
     * @param nonce  Output buffer (k_nonce_len bytes).
     */
    static void build_nonce(
        std::span<const std::uint8_t> addr, std::uint32_t seq,
        std::array<std::uint8_t, k_nonce_len>& nonce) noexcept;

    /**
     * @brief Check that @p new_seq is strictly greater than the stored value.
     *
     * Wraps at UINT32_MAX by accepting 0 only when stored is also 0.
     *
     * @param stored  Previously accepted sequence number.
     * @param new_seq Candidate sequence number from the advertisement.
     * @return true if @p new_seq is acceptable.
     */
    [[nodiscard]] static bool check_sequence(std::uint32_t stored,
                                             std::uint32_t new_seq) noexcept;

    CryptoBackend& crypto_;  ///< Platform AES-CCM backend (non-owning ref).
    EnoceanCallbacks callbacks_{};  ///< Application-provided callback table.
    bool initialised_{false};
    bool commissioning_enabled_{false};

    /// Fixed-size commissioned device table (no heap allocation).
    std::array<EnoceanDevice, k_max_devices> devices_{};
};

}  // namespace Enocean

// Template implementation must be visible at the point of instantiation.
#include "EnoceanDriver.ipp"
