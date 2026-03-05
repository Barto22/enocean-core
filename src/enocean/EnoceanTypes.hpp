/**
 * @file EnoceanTypes.hpp
 * @brief Shared data structures and callback types for the EnOcean driver.
 *
 * Mirrors the types from the NRF SDK bt_enocean.h but adapted for
 * the platform-agnostic C++23 driver.  All structures are POD-safe
 * (trivially copyable where possible) to satisfy MISRA-C++ Rule 6-2-2.
 *
 * EnOcean BLE protocol constants:
 *   - Manufacturer ID  : 0x03DA (EnOcean Alliance)
 *   - Max devices      : configurable via k_max_devices
 *   - Key length       : 16 bytes (AES-128)
 *   - Tag length       : 4 bytes  (CCM-4)
 *   - Nonce length     : 13 bytes
 *   - Seq num size     : 4 bytes
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Enocean {

/// EnOcean Alliance Bluetooth SIG Company Identifier (little-endian bytes).
inline constexpr std::uint16_t k_enocean_manufacturer_id{0x03DAU};

/// Length of an AES-128 key in bytes.
inline constexpr std::size_t k_key_len{16U};

/// AES-CCM tag (MIC) length used by EnOcean devices.
inline constexpr std::size_t k_tag_len{4U};

/// AES-CCM nonce length (addr[6] + seq[4] + pad[3]).
inline constexpr std::size_t k_nonce_len{13U};

/// BLE address length in bytes.
inline constexpr std::size_t k_addr_len{6U};

/// Maximum number of simultaneously commissioned EnOcean devices.
inline constexpr std::size_t k_max_devices{8U};

/// Top-left button (O side, channel A).
inline constexpr std::uint8_t k_switch_oa{0x01U};
/// Bottom-left button (I side, channel A).
inline constexpr std::uint8_t k_switch_ia{0x02U};
/// Top-right button (O side, channel B).
inline constexpr std::uint8_t k_switch_ob{0x04U};
/// Bottom-right button (I side, channel B).
inline constexpr std::uint8_t k_switch_ib{0x08U};
/// Top button alias for single-rocker.
inline constexpr std::uint8_t k_switch_o{k_switch_oa};
/// Bottom button alias for single-rocker.
inline constexpr std::uint8_t k_switch_i{k_switch_ia};

/**
 * @brief Button action reported by the EnOcean switch callback.
 */
enum class ButtonAction : std::uint8_t {
    Release = 0U,  ///< Button was released.
    Press = 1U,    ///< Button was pressed.
};

/**
 * @brief EnOcean device type determined from the BLE address bytes.
 */
enum class DeviceType : std::uint8_t {
    Switch = 0U,   ///< Double-rocker or single-rocker switch.
    Sensor = 1U,   ///< Environmental sensor (occupancy, light, etc.).
    Unknown = 2U,  ///< Unrecognised device type.
};

/**
 * @brief BLE address representation (6 bytes + type).
 *
 * addr[0] is the least-significant byte (LE wire format).
 */
struct BleAddr {
    std::array<std::uint8_t, k_addr_len>
        addr{};             ///< 6-byte BLE address; addr[0] is the LSB.
    std::uint8_t type{0U};  ///< 0 = public, 1 = random static.
};

/**
 * @brief Represents a commissioned EnOcean device.
 *
 * Stored in the driver's fixed-size device table.  The sequence number is
 * used to prevent replay attacks.
 */
struct EnoceanDevice {
    BleAddr ble_addr{};                         ///< Device BLE address.
    std::array<std::uint8_t, k_key_len> key{};  ///< AES-128 commissioning key.
    std::uint32_t seq_num{0U};  ///< Last accepted sequence number.
    std::int8_t rssi{0};        ///< Most recent RSSI in dBm.
    DeviceType type{
        DeviceType::Unknown};  ///< Device type (set during commissioning).
    bool active{false};        ///< Slot is in use.
};

/**
 * @brief Sensor data reported by the EnOcean sensor callback.
 *
 * Fields are optional because not all sensor types include all measurements.
 * Scaling follows the EnOcean BLE Sensor Protocol Specification.
 */
struct SensorData {
    std::optional<bool> occupancy{};                 ///< True = occupied.
    std::optional<std::uint16_t> light_sensor{};     ///< Ambient light in lux.
    std::optional<std::uint16_t> battery_voltage{};  ///< Battery voltage in mV.
    std::optional<std::uint16_t>
        light_solar_cell{};                    ///< Solar cell light in lux.
    std::optional<std::uint8_t> energy_lvl{};  ///< Energy level 0–100 %.
    std::optional<std::int16_t>
        temperature_cdeg{};  ///< Temperature in 0.01 °C (e.g. 2350 = 23.50 °C).
    std::optional<std::uint8_t>
        humidity{};                 ///< Relative humidity in % (raw / 2).
    std::optional<bool> contact{};  ///< Magnetic contact: true = closed.
    /// Acceleration sensor status: 0=unequipped, 1=inactive, 2=active,
    /// 3=disabled.
    std::optional<std::uint8_t> accel_status{};
    std::optional<std::int16_t>
        accel_x_cg{};  ///< X-axis acceleration in 0.01 g.
    std::optional<std::int16_t>
        accel_y_cg{};  ///< Y-axis acceleration in 0.01 g.
    std::optional<std::int16_t>
        accel_z_cg{};  ///< Z-axis acceleration in 0.01 g.
};

/**
 * @brief Called when a commissioned switch sends a button event.
 *
 * @param device   The device entry (address, key, rssi).
 * @param action   Press or release.
 * @param changed  Bitmask of buttons that changed state (k_switch_* values).
 * @param opt_data Pointer to optional payload bytes (may be nullptr).
 * @param opt_len  Length of optional payload.
 */
using ButtonCallback = void (*)(const EnoceanDevice& device,
                                ButtonAction action, std::uint8_t changed,
                                const std::uint8_t* opt_data,
                                std::size_t opt_len) noexcept;

/**
 * @brief Called when a commissioned sensor sends a data report.
 *
 * @param device   The device entry.
 * @param data     Parsed sensor measurements.
 * @param opt_data Pointer to optional payload bytes (may be nullptr).
 * @param opt_len  Length of optional payload.
 */
using SensorCallback = void (*)(const EnoceanDevice& device,
                                const SensorData& data,
                                const std::uint8_t* opt_data,
                                std::size_t opt_len) noexcept;

/**
 * @brief Called when a new device is successfully commissioned.
 * @param device The newly commissioned device entry.
 */
using CommissionedCallback = void (*)(const EnoceanDevice& device) noexcept;

/**
 * @brief Called when a device is decommissioned.
 * @param device The removed device entry.
 */
using DecommissionedCallback = void (*)(const EnoceanDevice& device) noexcept;

/**
 * @brief Called for each device loaded from persistent storage at startup.
 * @param device The loaded device entry.
 */
using LoadedCallback = void (*)(const EnoceanDevice& device) noexcept;

/**
 * @brief Aggregated callback table injected into the driver at initialisation.
 *
 * Any callback pointer left as nullptr is silently skipped.
 */
struct EnoceanCallbacks {
    ButtonCallback button{
        nullptr};  ///< Switch button event handler; may be nullptr.
    SensorCallback sensor{
        nullptr};  ///< Sensor data event handler; may be nullptr.
    CommissionedCallback commissioned{
        nullptr};  ///< New device commissioned; may be nullptr.
    DecommissionedCallback decommissioned{
        nullptr};  ///< Device removed; may be nullptr.
    LoadedCallback loaded{
        nullptr};  ///< Device loaded from storage; may be nullptr.
};

}  // namespace Enocean
