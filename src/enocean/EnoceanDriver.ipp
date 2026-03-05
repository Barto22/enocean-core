/**
 * @file EnoceanDriver.ipp
 * @brief Template method implementations for EnoceanDriver<CryptoBackend>.
 *
 * Included at the bottom of EnoceanDriver.hpp so the compiler sees the
 * definitions at every translation unit that instantiates the template.
 *
 * Manufacturer-specific payload layout (after the 2-byte Company ID 0x03DA).
 * All offsets are 0-based from the first byte after the Company ID.
 *
 *   Commissioning advertisement (switch or sensor, ≥22 bytes):
 *     Bytes 0..3  : sequence number (uint32, little-endian)
 *     Bytes 4..19 : AES-128 security key
 *     Bytes 20+   : optional device info / capabilities (ignored)
 *
 *   Data advertisement — switch/button (9..13 bytes, PTM-216B):
 *     Bytes 0..3  : sequence number (uint32, little-endian)
 *     Byte  4     : Switch Status  (bit0=ACTION_TYPE 1=press/0=release,
 *                                   bits[4:1]=active buttons A0/A1/B0/B1)
 *     Bytes 5..N-4: optional user data (0, 1, 2, or 4 bytes)
 *     Last 4 bytes: AES-CCM-4 tag (4-byte MIC)
 *
 *   Data advertisement — sensor (10–21 bytes):
 *     Bytes 0..3  : sequence number (uint32, little-endian)
 *     Bytes 4..N-4: encoded sensor values
 *     Last 4 bytes: AES-CCM-4 tag (4-byte MIC)
 *
 * Device type routing for known devices uses the stored DeviceType (set during
 * commissioning from the BLE address product-type bytes).
 * For unknown devices, payload size ≥22 triggers commissioning.
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <logging/Logger.hpp>

namespace Enocean {

namespace detail {

/// AD type for manufacturer-specific data.
inline constexpr std::uint8_t k_ad_type_manufacturer{0xFFU};

/// Minimum manufacturer-specific payload size for a commissioning
/// advertisement. seq(4) + key(16) + optional(≥0); threshold chosen safely
/// above data max (9).
inline constexpr std::size_t k_commissioning_payload_min{22U};

/// Minimum manufacturer-specific payload size for a switch button-event packet.
/// seq(4) + state(1) + optional(0..4) + tag(4) = 9..13 bytes.
inline constexpr std::size_t k_switch_data_len{9U};

/// Minimum manufacturer-specific payload size for any data advertisement.
/// seq(4) + data(1) + tag(4) = 9 bytes.
inline constexpr std::size_t k_data_payload_min{9U};

/// Offset of sequence number within the manufacturer-specific payload.
/// Payload starts immediately after the 2-byte Company ID.
inline constexpr std::size_t k_seq_offset{0U};

/// Offset of the AES-128 key within a commissioning payload.
/// seq(4) = 4 bytes before key.
inline constexpr std::size_t k_key_offset{4U};

/// Offset of the button-state byte within a switch data payload.
/// seq(4) = 4 bytes before state.
inline constexpr std::size_t k_switch_state_offset{4U};

/// Offset of the first sensor-data byte within a sensor data payload.
/// seq(4) = 4 bytes before sensor data.
inline constexpr std::size_t k_sensor_data_offset{4U};

/// BLE address byte [5] (MSB in LE wire order) for PTM-215B switch devices.
/// PTM-215B places company ID 0x03DA in the upper two address bytes:
///   addr[5] = 0x03 (MSB of 0x03DA), addr[4] = 0xDA (LSB of 0x03DA).
inline constexpr std::uint8_t k_switch_addr5{0x03U};
/// BLE address byte [4] for PTM-215B switch devices.
inline constexpr std::uint8_t k_switch_addr4{0xDAU};
/// BLE address byte [5] for PTM-216B switch devices (Product Type ID 0xE215).
inline constexpr std::uint8_t k_switch_ptm216b_addr5{0xE2U};
/// BLE address byte [4] for PTM-216B switch devices.
inline constexpr std::uint8_t k_switch_ptm216b_addr4{0x15U};
/// BLE address byte [5] (MSB in LE wire order) for sensor devices.
inline constexpr std::uint8_t k_sensor_addr5{0xE5U};
/// STM-550B commissioning telegram field identifier byte (TLV type 0x3E).
/// Appears at payload offset 4 — the byte immediately after the sequence
/// counter. When present: key is at offset 5. Absent (PTM-216B): key is at
/// k_key_offset (4).
inline constexpr std::uint8_t k_stm_commissioning_marker{0x3EU};
/// Manufacturer payload size for STM-550B commissioning:
/// seq(4) + 0x3E(1) + key(16) + addr(6) = 27.
inline constexpr std::size_t k_stm_commissioning_payload_len{27U};

/// Non-connectable undirected advertising PDU type (HCI value).
inline constexpr std::uint8_t k_adv_nonconn_ind{0x03U};

/// Random address type.
inline constexpr std::uint8_t k_addr_random{0x01U};

/// Bitmask to extract the 6-bit type_id from a sensor TLV header byte.
inline constexpr std::uint8_t k_tlv_type_mask{0x3FU};
/// Bit position of the 2-bit size field in a sensor TLV header byte.
inline constexpr std::uint8_t k_tlv_size_shift{6U};
/// Size-bits value indicating a variable-length TLV field.
inline constexpr std::uint8_t k_tlv_size_var{0x03U};
/// TLV type_id for the OPTIONAL_DATA field (variable-length, skip).
inline constexpr std::uint8_t k_tlv_type_optional{0x3CU};
/// TLV type_id for the embedded COMMISSIONING block (fixed 22 bytes, skip).
inline constexpr std::uint8_t k_tlv_type_commissioning{0x3EU};
/// Byte length of an embedded commissioning block inside a sensor packet.
inline constexpr std::size_t k_tlv_commissioning_block{22U};

}  // namespace detail

template <typename CryptoBackend>
[[nodiscard]] auto EnoceanDriver<CryptoBackend>::init() noexcept
    -> ErrorHandler<std::monostate, EnoceanError> {
    for (auto& dev : devices_) {
        dev = EnoceanDevice{};
    }
    commissioning_enabled_ = false;
    initialised_ = true;
    LOGGER_NOTICE("EnoceanDriver initialised (max %zu devices)", k_max_devices);
    return ErrorHandler<std::monostate, EnoceanError>(std::monostate{});
}

template <typename CryptoBackend>
void EnoceanDriver<CryptoBackend>::enable_commissioning() noexcept {
    commissioning_enabled_ = true;
    LOGGER_NOTICE("EnOcean commissioning enabled");
}

template <typename CryptoBackend>
void EnoceanDriver<CryptoBackend>::disable_commissioning() noexcept {
    commissioning_enabled_ = false;
    LOGGER_NOTICE("EnOcean commissioning disabled");
}

template <typename CryptoBackend>
bool EnoceanDriver<CryptoBackend>::commissioning_enabled() const noexcept {
    return commissioning_enabled_;
}

template <typename CryptoBackend>
std::size_t EnoceanDriver<CryptoBackend>::device_count() const noexcept {
    std::size_t count{0U};
    for (const auto& dev : devices_) {
        if (dev.active) {
            ++count;
        }
    }
    return count;
}

template <typename CryptoBackend>
void EnoceanDriver<CryptoBackend>::advertisement_cb(
    void* ctx, std::span<const std::uint8_t> addr, std::uint8_t addr_type,
    std::uint8_t adv_type, std::int8_t rssi,
    std::span<const std::uint8_t> data) noexcept {
    if (ctx == nullptr) {
        return;
    }
    auto* driver{static_cast<EnoceanDriver<CryptoBackend>*>(ctx)};
    driver->process_advertisement(addr, addr_type, adv_type, rssi, data);
}

template <typename CryptoBackend>
void EnoceanDriver<CryptoBackend>::process_advertisement(
    std::span<const std::uint8_t> addr, std::uint8_t addr_type,
    std::uint8_t adv_type, std::int8_t rssi,
    std::span<const std::uint8_t> data) noexcept {
    if (!initialised_) {
        return;
    }

    if (addr.size() != k_addr_len) {
        return;
    }

    if ((adv_type != detail::k_adv_nonconn_ind) ||
        (addr_type != detail::k_addr_random)) {
        return;
    }

    const auto mfr_payload{extract_manufacturer_data(data)};
    if (mfr_payload.empty()) {
        return;
    }

    if (mfr_payload.size() < detail::k_data_payload_min) {
        return;
    }

    if (auto* device{find_device(addr)}; device != nullptr) {
        device->rssi = rssi;
        if (device->type == DeviceType::Switch) {
            (void)handle_switch(*device, mfr_payload);
        } else {
            (void)handle_sensor(*device, mfr_payload);
        }
        return;
    }

    if ((mfr_payload.size() >= detail::k_commissioning_payload_min) &&
        commissioning_enabled_) {
        (void)handle_commissioning(addr, addr_type, rssi, mfr_payload);
    }
}

template <typename CryptoBackend>
EnoceanDevice* EnoceanDriver<CryptoBackend>::find_device(
    std::span<const std::uint8_t> addr) noexcept {
    for (auto& dev : devices_) {
        if (!dev.active) {
            continue;
        }
        if (std::equal(addr.begin(), addr.end(), dev.ble_addr.addr.begin())) {
            return &dev;
        }
    }
    return nullptr;
}

template <typename CryptoBackend>
EnoceanDevice* EnoceanDriver<CryptoBackend>::allocate_device() noexcept {
    for (auto& dev : devices_) {
        if (!dev.active) {
            return &dev;
        }
    }
    return nullptr;
}

template <typename CryptoBackend>
DeviceType EnoceanDriver<CryptoBackend>::detect_device_type(
    std::span<const std::uint8_t> addr) noexcept {
    if (addr.size() < k_addr_len) {
        return DeviceType::Unknown;
    }

    if (((addr[5U] == detail::k_switch_addr5) &&
         (addr[4U] == detail::k_switch_addr4)) ||
        ((addr[5U] == detail::k_switch_ptm216b_addr5) &&
         (addr[4U] == detail::k_switch_ptm216b_addr4))) {
        return DeviceType::Switch;
    }
    if (addr[5U] == detail::k_sensor_addr5) {
        return DeviceType::Sensor;
    }
    return DeviceType::Unknown;
}

template <typename CryptoBackend>
std::span<const std::uint8_t>
EnoceanDriver<CryptoBackend>::extract_manufacturer_data(
    std::span<const std::uint8_t> adv_data) noexcept {
    std::size_t i{0U};
    while (i < adv_data.size()) {
        const std::uint8_t len{adv_data[i]};
        if (len == 0U) {
            break;
        }
        if ((i + static_cast<std::size_t>(len)) > adv_data.size()) {
            break;
        }
        ++i;
        const std::uint8_t type{adv_data[i]};
        if (type == detail::k_ad_type_manufacturer) {
            if ((adv_data.size() - i) < 3U) {
                return {};
            }
            const std::uint16_t company_id{static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(adv_data[i + 1U]) |
                (static_cast<std::uint16_t>(adv_data[i + 2U]) << 8U))};
            if (company_id == k_enocean_manufacturer_id) {
                if (static_cast<std::size_t>(len) < 3U) {
                    return {};
                }
                const std::size_t payload_start{i + 3U};
                const std::size_t payload_len{static_cast<std::size_t>(len) -
                                              3U};
                return adv_data.subspan(payload_start, payload_len);
            }
        }
        i += static_cast<std::size_t>(len);
    }
    return {};
}

template <typename CryptoBackend>
void EnoceanDriver<CryptoBackend>::build_nonce(
    std::span<const std::uint8_t> addr, std::uint32_t seq,
    std::array<std::uint8_t, k_nonce_len>& nonce) noexcept {
    for (std::size_t i{0U}; i < k_addr_len; ++i) {
        nonce[i] = addr[i];
    }
    nonce[6U] = static_cast<std::uint8_t>(seq & 0xFFU);
    nonce[7U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    nonce[8U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    nonce[9U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    nonce[10U] = 0U;
    nonce[11U] = 0U;
    nonce[12U] = 0U;
}

template <typename CryptoBackend>
bool EnoceanDriver<CryptoBackend>::check_sequence(
    std::uint32_t stored, std::uint32_t new_seq) noexcept {
    return new_seq > stored;
}

template <typename CryptoBackend>
bool EnoceanDriver<CryptoBackend>::handle_commissioning(
    std::span<const std::uint8_t> addr, std::uint8_t addr_type,
    std::int8_t rssi, std::span<const std::uint8_t> mfr_payload) noexcept {
    if (mfr_payload.size() < detail::k_commissioning_payload_min) {
        return false;
    }

    const std::uint32_t seq{
        static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset]) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 1U])
         << 8U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 2U])
         << 16U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 3U])
         << 24U)};

    const auto dev_type{detect_device_type(addr)};

    // STM-550B commissioning is identified by an exact 27-byte payload and the
    // 0x3E marker at byte[4].  Using '>=' instead of '==' would cause false
    // positives for PTM-216B packets (22 bytes) that happen to have 0x3E as
    // the first byte of their AES-128 key (at the same offset), resulting in
    // the key being extracted from the wrong offset.
    const bool stm_format{
        (mfr_payload.size() == detail::k_stm_commissioning_payload_len) &&
        (mfr_payload[detail::k_key_offset] ==
         detail::k_stm_commissioning_marker)};
    const std::size_t key_offset{stm_format ? (detail::k_key_offset + 1U)
                                            : detail::k_key_offset};

    if ((dev_type == DeviceType::Sensor) && !stm_format) {
        return false;
    }

    if (key_offset + k_key_len > mfr_payload.size()) {
        return false;
    }

    // Re-commission: update key/seq/rssi if device is already known.
    // Key must be refreshed so that a fresh commissioning packet (e.g. from a
    // button press) can correct a device that was previously stored with a
    // stale or wrong key.  The sequence check is relaxed here: a device that
    // has power-cycled will restart its counter from a low value and must be
    // allowed to re-commission; blocking it would leave the device permanently
    // unusable until manually removed from the table.
    EnoceanDevice* device{find_device(addr)};
    if (device != nullptr) {
        if (!check_sequence(device->seq_num, seq)) {
            LOGGER_WARNING(
                "EnoceanDriver: re-commissioning with non-increasing seq "
                "(stored=%lu new=%lu) — device may have reset",
                static_cast<unsigned long>(device->seq_num),
                static_cast<unsigned long>(seq));
        }
        device->seq_num = seq;
        device->rssi = rssi;
        std::copy(mfr_payload.begin() + static_cast<std::ptrdiff_t>(key_offset),
                  mfr_payload.begin() +
                      static_cast<std::ptrdiff_t>(key_offset + k_key_len),
                  device->key.begin());
        return true;
    }

    device = allocate_device();
    if (device == nullptr) {
        LOGGER_ERROR("EnoceanDriver: device table full, cannot commission");
        return false;
    }

    device->active = true;
    device->seq_num = seq;
    device->rssi = rssi;
    device->type = dev_type;
    device->ble_addr.type = addr_type;
    std::copy(addr.begin(), addr.end(), device->ble_addr.addr.begin());
    std::copy(mfr_payload.begin() + static_cast<std::ptrdiff_t>(key_offset),
              mfr_payload.begin() +
                  static_cast<std::ptrdiff_t>(key_offset + k_key_len),
              device->key.begin());

    LOGGER_NOTICE(
        "EnoceanDriver: device commissioned "
        "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
        addr[5U], addr[4U], addr[3U], addr[2U], addr[1U], addr[0U]);

    if (callbacks_.commissioned != nullptr) {
        callbacks_.commissioned(*device);
    }
    return true;
}

template <typename CryptoBackend>
bool EnoceanDriver<CryptoBackend>::handle_switch(
    EnoceanDevice& device, std::span<const std::uint8_t> mfr_payload) noexcept {
    if (mfr_payload.size() < detail::k_data_payload_min) {
        return false;
    }

    const std::uint32_t seq{
        static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset]) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 1U])
         << 8U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 2U])
         << 16U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 3U])
         << 24U)};

    if (!check_sequence(device.seq_num, seq)) {
        LOGGER_WARNING("EnoceanDriver: replay detected (switch)");
        return false;
    }

    // NCS validates the payload size range: seq(4)+state(1)+opt(0/1/2/4)+tag(4)
    // = 9..13 bytes.  Anything outside this range is not a valid switch packet.
    constexpr std::size_t k_switch_payload_max{13U};
    if (mfr_payload.size() > k_switch_payload_max) {
        return false;
    }

    const std::uint8_t state_byte_pre{mfr_payload[detail::k_switch_state_offset]};

    // NCS rejects packets where the reserved top 3 bits of the status byte
    // are non-zero (bits[7:5] must be 0).
    constexpr std::uint8_t k_status_reserved_mask{0xE0U};
    if ((state_byte_pre & k_status_reserved_mask) != 0U) {
        return false;
    }

    const std::size_t tag_offset{mfr_payload.size() - k_tag_len};
    const auto tag_span{mfr_payload.subspan(tag_offset, k_tag_len)};
    const auto payload_span{mfr_payload.subspan(0U, tag_offset)};

    std::array<std::uint8_t, k_nonce_len> nonce{};
    build_nonce(std::span<const std::uint8_t>(device.ble_addr.addr), seq,
                nonce);

    std::array<std::uint8_t, k_tag_len> tag_arr{};
    std::copy(tag_span.begin(), tag_span.end(), tag_arr.begin());

    const bool auth_ok{crypto_.aes_ccm_decrypt(
        std::span<const std::uint8_t>(device.key),
        std::span<const std::uint8_t>(nonce), payload_span,
        std::span<const std::uint8_t>{}, tag_arr, std::span<std::uint8_t>{})};

    if (!auth_ok) {
        LOGGER_WARNING("EnoceanDriver: switch auth failed");
        return false;
    }

    device.seq_num = seq;

    const std::uint8_t state_byte{mfr_payload[detail::k_switch_state_offset]};
    const bool pressed{(state_byte & 0x01U) != 0U};
    const std::uint8_t buttons{
        static_cast<std::uint8_t>((state_byte >> 1U) & 0x0FU)};

    const ButtonAction action{pressed ? ButtonAction::Press
                                      : ButtonAction::Release};

    const std::size_t opt_offset{detail::k_switch_state_offset + 1U};
    const std::uint8_t* opt_ptr{nullptr};
    std::size_t opt_len{0U};
    if (tag_offset > opt_offset) {
        opt_len = tag_offset - opt_offset;
        // NCS only accepts optional data lengths of 0, 1, 2, or 4 bytes;
        // length 3 is not defined in the EnOcean switch protocol.
        if (opt_len != 1U && opt_len != 2U && opt_len != 4U) {
            return false;
        }
        opt_ptr = mfr_payload.data() + opt_offset;
    }

    if (callbacks_.button != nullptr) {
        callbacks_.button(device, action, buttons, opt_ptr, opt_len);
    }
    return true;
}

template <typename CryptoBackend>
bool EnoceanDriver<CryptoBackend>::handle_sensor(
    EnoceanDevice& device, std::span<const std::uint8_t> mfr_payload) noexcept {
    if (mfr_payload.size() < detail::k_data_payload_min) {
        return false;
    }

    const std::uint32_t seq{
        static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset]) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 1U])
         << 8U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 2U])
         << 16U) |
        (static_cast<std::uint32_t>(mfr_payload[detail::k_seq_offset + 3U])
         << 24U)};

    LOGGER_NOTICE(
        "EnoceanDriver: sensor pkt %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX "
        "payload[%zu] seq=%lu",
        device.ble_addr.addr[5U], device.ble_addr.addr[4U],
        device.ble_addr.addr[3U], device.ble_addr.addr[2U],
        device.ble_addr.addr[1U], device.ble_addr.addr[0U], mfr_payload.size(),
        static_cast<unsigned long>(seq));

    if (!check_sequence(device.seq_num, seq)) {
        LOGGER_WARNING("EnoceanDriver: replay detected (sensor)");
        return false;
    }

    const std::size_t tag_offset{mfr_payload.size() - k_tag_len};
    const auto tag_span{mfr_payload.subspan(tag_offset, k_tag_len)};
    const auto payload_span{mfr_payload.subspan(0U, tag_offset)};

    std::array<std::uint8_t, k_nonce_len> nonce{};
    build_nonce(std::span<const std::uint8_t>(device.ble_addr.addr), seq,
                nonce);

    std::array<std::uint8_t, k_tag_len> tag_arr{};
    std::copy(tag_span.begin(), tag_span.end(), tag_arr.begin());

    const bool auth_ok{crypto_.aes_ccm_decrypt(
        std::span<const std::uint8_t>(device.key),
        std::span<const std::uint8_t>(nonce), payload_span,
        std::span<const std::uint8_t>{}, tag_arr, std::span<std::uint8_t>{})};

    if (!auth_ok) {
        LOGGER_WARNING("EnoceanDriver: sensor auth failed");
        return false;
    }

    device.seq_num = seq;

    SensorData sensor_data{};

    if (mfr_payload.size() > detail::k_sensor_data_offset) {
        const auto sensor_bytes{
            mfr_payload.subspan(detail::k_sensor_data_offset,
                                tag_offset - detail::k_sensor_data_offset)};

        std::size_t j{0U};
        while (j < sensor_bytes.size()) {
            const std::uint8_t header{sensor_bytes[j++]};
            const std::uint8_t type_id{
                static_cast<std::uint8_t>(header & detail::k_tlv_type_mask)};
            const std::uint8_t size_bits{
                static_cast<std::uint8_t>(header >> detail::k_tlv_size_shift)};

            if (type_id == detail::k_tlv_type_commissioning) {
                j = (j + detail::k_tlv_commissioning_block <=
                     sensor_bytes.size())
                        ? j + detail::k_tlv_commissioning_block
                        : sensor_bytes.size();
                continue;
            }

            if ((size_bits == detail::k_tlv_size_var) ||
                (type_id == detail::k_tlv_type_optional)) {
                if (j >= sensor_bytes.size()) {
                    break;
                }
                const std::size_t vlen{
                    static_cast<std::size_t>(sensor_bytes[j++])};
                if (vlen > (sensor_bytes.size() - j)) {
                    break;
                }
                j += vlen;
                continue;
            }

            const std::size_t val_len{static_cast<std::size_t>(1U)
                                      << size_bits};
            if (j + val_len > sensor_bytes.size()) {
                break;
            }

            std::uint32_t value{0U};
            for (std::size_t b{0U}; b < val_len; ++b) {
                value |= static_cast<std::uint32_t>(sensor_bytes[j + b])
                         << (8U * b);
            }
            j += val_len;

            switch (type_id) {
                case 0x00U: {  // TEMPERATURE: int16 LE, raw/100 = °C
                    sensor_data.temperature_cdeg =
                        static_cast<std::int16_t>(value & 0xFFFFU);
                    break;
                }
                case 0x01U:  // BATTERY: uint16 LE, raw/2 = mV
                    sensor_data.battery_voltage =
                        static_cast<std::uint16_t>(value / 2U);
                    break;
                case 0x02U:  // ENERGY_LEVEL: uint8, raw/2 = %
                    sensor_data.energy_lvl =
                        static_cast<std::uint8_t>(value / 2U);
                    break;
                case 0x04U:  // LIGHT_SOLAR_CELL: uint16 LE, lux
                    sensor_data.light_solar_cell =
                        static_cast<std::uint16_t>(value);
                    break;
                case 0x05U:  // LIGHT_SENSOR: uint16 LE, lux
                    sensor_data.light_sensor =
                        static_cast<std::uint16_t>(value);
                    break;
                case 0x06U:  // HUMIDITY: uint8, raw/2 = %RH
                    sensor_data.humidity =
                        static_cast<std::uint8_t>((value & 0xFFU) / 2U);
                    break;
                case 0x0AU: {  // ACCELERATION: uint32 little-endian — bit layout
                               // per STM-550B spec. The common loop already
                               // assembled the bytes as LE into 'value'.
                    if (val_len == 4U) {
                        const std::uint32_t le_value{
                            static_cast<std::uint32_t>(value)};
                        // bits[31:30]=status, bits[29:20]=x, bits[19:10]=y,
                        // bits[9:0]=z; each 10-bit axis is signed with
                        // offset 512, unit = 0.01 g.
                        sensor_data.accel_status =
                            static_cast<std::uint8_t>(le_value >> 30U);
                        sensor_data.accel_x_cg = static_cast<std::int16_t>(
                            static_cast<int>((le_value >> 20U) & 0x3FFU) -
                            512);
                        sensor_data.accel_y_cg = static_cast<std::int16_t>(
                            static_cast<int>((le_value >> 10U) & 0x3FFU) -
                            512);
                        sensor_data.accel_z_cg = static_cast<std::int16_t>(
                            static_cast<int>(le_value & 0x3FFU) - 512);
                    }
                    break;
                }
                case 0x20U:  // OCCUPANCY: per EnOcean BLE Sensor Protocol spec:
                             //   0x00 = sensor not fitted
                             //   0x01 = not occupied
                             //   0x02 = occupied
                    sensor_data.occupancy = (value == 0x02U);
                    break;
                case 0x23U: {  // MAGNET_CONTACT: 0x01=open, 0x02=closed
                    const std::uint8_t v{
                        static_cast<std::uint8_t>(value & 0xFFU)};
                    sensor_data.contact = (v == 0x02U);
                    break;
                }
                default:
                    // Unknown type — skip (size already consumed above).
                    break;
            }
        }
    }

    if (callbacks_.sensor != nullptr) {
        callbacks_.sensor(device, sensor_data, nullptr, 0U);
    }
    return true;
}

}  // namespace Enocean
