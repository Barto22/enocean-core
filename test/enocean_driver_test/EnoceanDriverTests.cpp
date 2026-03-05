/**
 * @file EnoceanDriverTests.cpp
 * @brief Unit tests for Enocean::EnoceanDriver<CryptoBackend>.
 *
 * A minimal mock crypto backend is used so tests run without OpenSSL.
 * The mock returns configurable pass/fail responses, allowing both the
 * happy path and the authentication-failure branch to be exercised.
 *
 * Test coverage:
 * -# Driver initialises cleanly and reports zero devices.
 * -# process_advertisement ignores non-EnOcean packets (wrong company ID).
 * -# process_advertisement ignores connectable advertisements.
 * -# Commissioning adds a device and fires the commissioned callback.
 * -# Commissioning is rejected when commissioning mode is disabled.
 * -# Replay attacks (same sequence number) are rejected.
 * -# Switch button events are dispatched with correct action and bitmask.
 * -# Authentication failure suppresses the button callback.
 * -# Sensor data fields are parsed and dispatched.
 * -# Device table full: exceeding k_max_devices fails gracefully.
 */

/// @cond TESTS

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include "ICrypto.hpp"
#include "enocean/EnoceanDriver.hpp"
#include "enocean/EnoceanTypes.hpp"

namespace {

/**
 * @brief Mock that returns a configurable boolean and records the last call.
 * Records the key and nonce passed to aes_ccm_decrypt_impl for verification.
 */
class MockCrypto final : public Crypto::ICrypto<MockCrypto> {
   public:
    bool auth_result{
        true};  ///< Returned by every call to aes_ccm_decrypt_impl.
    bool was_called{false};
    std::array<std::uint8_t, Enocean::k_key_len> last_key{};
    std::array<std::uint8_t, Enocean::k_nonce_len> last_nonce{};

    void reset_recording() noexcept {
        was_called = false;
        last_key = {};
        last_nonce = {};
    }

    [[nodiscard]] bool aes_ccm_decrypt_impl(
        std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
        [[maybe_unused]] std::span<const std::uint8_t> aad,
        [[maybe_unused]] std::span<const std::uint8_t> ciphertext,
        [[maybe_unused]] std::array<std::uint8_t, Enocean::k_tag_len> tag,
        [[maybe_unused]] std::span<std::uint8_t> plaintext) noexcept {
        was_called = true;
        std::copy(key.begin(), key.end(), last_key.begin());
        std::copy(nonce.begin(), nonce.end(), last_nonce.begin());
        return auth_result;
    }
};

static_assert(Crypto::CryptoImplementation<MockCrypto>,
              "MockCrypto must satisfy CryptoImplementation concept");

struct CapturedButton {
    bool called{false};
    Enocean::ButtonAction action{Enocean::ButtonAction::Release};
    std::uint8_t changed{0U};
    std::size_t opt_len{0U};
};

struct CapturedSensor {
    bool called{false};
    Enocean::SensorData data{};
};

struct CapturedDevice {
    bool called{false};
    std::array<std::uint8_t, Enocean::k_addr_len> addr{};
};

// Global state shared between callbacks and tests.
CapturedButton g_button{};
CapturedSensor g_sensor{};
CapturedDevice g_commissioned{};
CapturedDevice g_decommissioned{};

void reset_captures() noexcept {
    g_button = CapturedButton{};
    g_sensor = CapturedSensor{};
    g_commissioned = CapturedDevice{};
    g_decommissioned = CapturedDevice{};
}

void cb_button(const Enocean::EnoceanDevice& dev, Enocean::ButtonAction action,
               std::uint8_t changed, [[maybe_unused]] const std::uint8_t*,
               std::size_t opt_len) noexcept {
    g_button.called = true;
    g_button.action = action;
    g_button.changed = changed;
    g_button.opt_len = opt_len;
    (void)dev;
}

void cb_sensor(const Enocean::EnoceanDevice& dev,
               const Enocean::SensorData& data,
               [[maybe_unused]] const std::uint8_t*,
               [[maybe_unused]] std::size_t) noexcept {
    g_sensor.called = true;
    g_sensor.data = data;
    (void)dev;
}

void cb_commissioned(const Enocean::EnoceanDevice& dev) noexcept {
    g_commissioned.called = true;
    std::copy(dev.ble_addr.addr.begin(), dev.ble_addr.addr.end(),
              g_commissioned.addr.begin());
}

void cb_decommissioned(const Enocean::EnoceanDevice& dev) noexcept {
    g_decommissioned.called = true;
    (void)dev;
}

/// EnOcean switch address: bytes [4]=0xDA, [5]=0x03.
constexpr std::array<std::uint8_t, 6U> k_switch_addr{0x01U, 0x02U, 0x03U,
                                                     0x04U, 0xDAU, 0x03U};

/// PTM-216B switch: Product Type ID 0xE215 → addr[5]=0xE2, addr[4]=0x15.
constexpr std::array<std::uint8_t, 6U> k_switch_ptm216b_addr{
    0x01U, 0x02U, 0x03U, 0x04U, 0x15U, 0xE2U};

/// STM-550B sensor: addr[5]=0xE5.
constexpr std::array<std::uint8_t, 6U> k_stm550b_addr{0x01U, 0x02U, 0x03U,
                                                      0x04U, 0x05U, 0xE5U};

/// Unknown device type address (neither switch nor sensor pattern).
constexpr std::array<std::uint8_t, 6U> k_unknown_type_addr{0x01U, 0x02U, 0x03U,
                                                           0x04U, 0xBBU, 0xAAU};

/// Build an AD payload with a manufacturer-specific record.
/// @param company_id  2-byte company ID (LE).
/// @param payload     Bytes after company ID.
std::vector<std::uint8_t> make_adv(std::uint16_t company_id,
                                   std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> adv{};
    // Length: 1 (type) + 2 (company id) + payload.
    const auto len{static_cast<std::uint8_t>(1U + 2U + payload.size())};
    adv.push_back(len);
    adv.push_back(0xFFU);  // AD type: manufacturer specific
    adv.push_back(static_cast<std::uint8_t>(company_id & 0xFFU));
    adv.push_back(static_cast<std::uint8_t>((company_id >> 8U) & 0xFFU));
    for (auto b : payload) {
        adv.push_back(b);
    }
    return adv;
}

/// Build a commissioning payload matching the PTM-216B real format:
///   seq(4) + key(16) + addr(6) = 26 bytes.
/// The driver requires ≥22 bytes (k_commissioning_payload_min) to accept it.
std::array<std::uint8_t, 26U> make_commissioning_payload(std::uint32_t seq) {
    std::array<std::uint8_t, 26U> p{};
    // Sequence number LE at offset 0.
    p[0U] = static_cast<std::uint8_t>(seq & 0xFFU);
    p[1U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    p[2U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    p[3U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    // Key bytes [4..19] — distinct non-zero pattern.
    for (std::size_t i{4U}; i < 20U; ++i) {
        p[i] = static_cast<std::uint8_t>(i + 0x10U);
    }
    // Device address bytes [20..25] (present in real packets; ignored by
    // driver).
    p[20U] = 0x01U;
    p[21U] = 0x02U;
    p[22U] = 0x03U;
    p[23U] = 0x04U;
    p[24U] = 0xDAU;
    p[25U] = 0x03U;
    return p;
}

/// Build a switch data payload: seq(4)+state(1)+tag(4) = 9 bytes.
/// make_adv() prepends LEN+TYPE+CompanyID(4 bytes); after wrapping the full AD
/// record is 13 bytes and k_switch_state_offset=8 (header+seq) points at state.
std::array<std::uint8_t, 9U> make_switch_payload(std::uint32_t seq,
                                                 std::uint8_t state) {
    std::array<std::uint8_t, 9U> p{};
    // Seq LE at [0..3].
    p[0U] = static_cast<std::uint8_t>(seq & 0xFFU);
    p[1U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    p[2U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    p[3U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    p[4U] = state;  // button state byte at k_switch_state_offset
    // tag bytes [5..8] = 0x00 (mock crypto ignores them).
    return p;
}

/// Build a switch payload with optional user data: seq(4)+state(1)+opt+tag(4).
/// opt_bytes is inserted between the state byte and the 4-byte tag.
std::vector<std::uint8_t> make_switch_payload_with_opt(
    std::uint32_t seq, std::uint8_t state,
    std::span<const std::uint8_t> opt_bytes) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(seq & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 8U) & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 16U) & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 24U) & 0xFFU));
    p.push_back(state);
    for (auto b : opt_bytes) {
        p.push_back(b);
    }
    // 4-byte tag (all zero).
    p.push_back(0x00U);
    p.push_back(0x00U);
    p.push_back(0x00U);
    p.push_back(0x00U);
    return p;
}

/// Build a sensor data payload: seq(4) + sensor_tlv_bytes + tag(4).
std::vector<std::uint8_t> make_sensor_payload(
    std::uint32_t seq, std::span<const std::uint8_t> tlv_bytes) {
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(seq & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 8U) & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 16U) & 0xFFU));
    p.push_back(static_cast<std::uint8_t>((seq >> 24U) & 0xFFU));
    for (auto b : tlv_bytes) {
        p.push_back(b);
    }
    p.push_back(0x00U);
    p.push_back(0x00U);
    p.push_back(0x00U);
    p.push_back(0x00U);
    return p;
}

/// Build a commissioning payload with exactly @p total_size bytes (≥22).
/// seq at [0..3], key at [4..19], rest zero-padded.
std::vector<std::uint8_t> make_commissioning_payload_sized(
    std::uint32_t seq, std::size_t total_size) {
    std::vector<std::uint8_t> p(total_size, 0x00U);
    p[0U] = static_cast<std::uint8_t>(seq & 0xFFU);
    p[1U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    p[2U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    p[3U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    for (std::size_t i{4U}; (i < 20U) && (i < total_size); ++i) {
        p[i] = static_cast<std::uint8_t>(i + 0x10U);
    }
    return p;
}

/// Build a PTM-216B commissioning payload (26 bytes, key at offset 4).
std::array<std::uint8_t, 26U> make_ptm216b_commissioning(
    std::uint32_t seq,
    const std::array<std::uint8_t, Enocean::k_key_len>& key) {
    std::array<std::uint8_t, 26U> p{};
    p[0U] = static_cast<std::uint8_t>(seq & 0xFFU);
    p[1U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    p[2U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    p[3U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    std::copy(key.begin(), key.end(), p.begin() + 4);
    return p;
}

/// Build an STM-550B commissioning payload (27 bytes:
/// seq+0x3E+key(16)+addr(6)). The 0x3E marker at offset 4 shifts the key to
/// offset 5.
std::array<std::uint8_t, 27U> make_stm550b_commissioning(
    std::uint32_t seq,
    const std::array<std::uint8_t, Enocean::k_key_len>& key) {
    std::array<std::uint8_t, 27U> p{};
    p[0U] = static_cast<std::uint8_t>(seq & 0xFFU);
    p[1U] = static_cast<std::uint8_t>((seq >> 8U) & 0xFFU);
    p[2U] = static_cast<std::uint8_t>((seq >> 16U) & 0xFFU);
    p[3U] = static_cast<std::uint8_t>((seq >> 24U) & 0xFFU);
    p[4U] = 0x3EU;  // STM-550B commissioning marker
    std::copy(key.begin(), key.end(), p.begin() + 5);
    // addr bytes at [21..26] — not used by driver.
    p[21U] = 0x01U;
    p[22U] = 0x02U;
    p[23U] = 0x03U;
    p[24U] = 0x04U;
    p[25U] = 0x05U;
    p[26U] = 0xE5U;
    return p;
}

}  // namespace

class EnoceanDriverTest : public ::testing::Test {
   protected:
    void SetUp() override {
        reset_captures();
        callbacks_ = Enocean::EnoceanCallbacks{
            .button = cb_button,
            .sensor = cb_sensor,
            .commissioned = cb_commissioned,
            .decommissioned = cb_decommissioned,
            .loaded = nullptr,
        };
        driver_ = std::make_unique<Enocean::EnoceanDriver<MockCrypto>>(
            crypto_, callbacks_);
        (void)driver_->init();
    }

    void TearDown() override { driver_.reset(); }

    /// Helper: commission a device with the given address.
    /// Automatically selects the correct commissioning format:
    /// - STM-550B sensors (addr[5]==0xE5) use the 27-byte STM format
    ///   (seq+0x3E+key+addr) required by the driver's sensor guard.
    /// - All other devices use the 26-byte PTM format (seq+key+addr).
    void commission_device(std::span<const std::uint8_t> addr,
                           std::uint32_t seq = 1U) {
        driver_->enable_commissioning();
        std::vector<std::uint8_t> adv{};
        if ((addr.size() >= Enocean::k_addr_len) && (addr[5U] == 0xE5U)) {
            constexpr std::array<std::uint8_t, Enocean::k_key_len> key{};
            const auto cp{make_stm550b_commissioning(seq, key)};
            adv = make_adv(Enocean::k_enocean_manufacturer_id,
                           std::span<const std::uint8_t>(cp));
        } else {
            const auto cp{make_commissioning_payload(seq)};
            adv = make_adv(Enocean::k_enocean_manufacturer_id,
                           std::span<const std::uint8_t>(cp));
        }
        driver_->process_advertisement(
            addr, 0x01U, 0x03U, -70,
            std::span<const std::uint8_t>(adv.data(), adv.size()));
    }

    /// Helper: send a commissioning advertisement for k_switch_addr.
    void commission_switch(std::uint32_t seq = 1U) {
        commission_device(std::span<const std::uint8_t>(k_switch_addr), seq);
    }

    /// Helper: send a switch data packet and return to the driver.
    void send_switch(std::span<const std::uint8_t> addr, std::uint32_t seq,
                     std::uint8_t state) {
        const auto sp{make_switch_payload(seq, state)};
        const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                                std::span<const std::uint8_t>(sp))};
        driver_->process_advertisement(
            addr, 0x01U, 0x03U, -60,
            std::span<const std::uint8_t>(adv.data(), adv.size()));
    }

    /// Helper: send a sensor data packet to the driver.
    void send_sensor(std::span<const std::uint8_t> addr, std::uint32_t seq,
                     std::span<const std::uint8_t> tlv_bytes) {
        const auto sp{make_sensor_payload(seq, tlv_bytes)};
        const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                                std::span<const std::uint8_t>(sp))};
        driver_->process_advertisement(
            addr, 0x01U, 0x03U, -60,
            std::span<const std::uint8_t>(adv.data(), adv.size()));
    }

    MockCrypto crypto_{};
    Enocean::EnoceanCallbacks callbacks_{};
    std::unique_ptr<Enocean::EnoceanDriver<MockCrypto>> driver_{};
};

TEST_F(EnoceanDriverTest, InitReportsZeroDevices) {
    EXPECT_EQ(driver_->device_count(), 0U);
    EXPECT_FALSE(driver_->commissioning_enabled());
}

TEST_F(EnoceanDriverTest, IgnoresNonEnOceanManufacturerId) {
    constexpr std::uint16_t wrong_id{0x0059U};  // Nordic Semiconductor
    const std::array<std::uint8_t, 4U> payload{0x01U, 0x02U, 0x03U, 0x04U};
    const auto adv{make_adv(wrong_id, std::span<const std::uint8_t>(payload))};
    driver_->enable_commissioning();
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 0U);
    EXPECT_FALSE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, IgnoresConnectableAdvertisements) {
    const auto cp{make_commissioning_payload(1U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->enable_commissioning();
    // adv_type 0x00 = ADV_IND (connectable) — should be ignored.
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x00U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 0U);
}

TEST_F(EnoceanDriverTest, CommissioningAddsDevice) {
    commission_switch(10U);
    EXPECT_EQ(driver_->device_count(), 1U);
    EXPECT_TRUE(g_commissioned.called);
    EXPECT_EQ(g_commissioned.addr, k_switch_addr);
}

TEST_F(EnoceanDriverTest, CommissioningRejectedWhenDisabled) {
    driver_->disable_commissioning();
    const auto cp{make_commissioning_payload(1U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 0U);
    EXPECT_FALSE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, SwitchButtonPressDispatched) {
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    // seq=2 (> 1). PTM-216B state byte encoding (section 4.6.2):
    //   bit0=1 (Press Action), bit1=A0/OA, bit3=B0/OB.
    //   Buttons nibble after >>1: bit0=OA, bit2=OB → 0x05 =
    //   k_switch_oa|k_switch_ob.
    constexpr std::uint8_t state{0x01U | (Enocean::k_switch_oa << 1U) |
                                 (Enocean::k_switch_ob << 1U)};
    const auto sp{make_switch_payload(2U, state)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};

    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Press);
    EXPECT_EQ(
        g_button.changed,
        static_cast<std::uint8_t>(Enocean::k_switch_oa | Enocean::k_switch_ob));
}

TEST_F(EnoceanDriverTest, SwitchButtonReleaseDispatched) {
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    // PTM-216B state byte: bit0=0 (Release Action), bit2=A1/IA → state=0x04.
    constexpr std::uint8_t state{
        static_cast<std::uint8_t>(Enocean::k_switch_ia << 1U)};
    const auto sp{make_switch_payload(2U, state)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};

    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Release);
}

TEST_F(EnoceanDriverTest, ReplayAttackRejected) {
    commission_switch(5U);
    reset_captures();
    driver_->disable_commissioning();

    // seq=3 < 5: replay.
    const auto sp{make_switch_payload(3U, 0x80U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};

    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, AuthenticationFailureSuppressesCallback) {
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();
    crypto_.auth_result = false;  // Mock will reject every tag.

    const auto sp{make_switch_payload(2U, 0x80U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};

    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, DeviceTableFullRejected) {
    driver_->enable_commissioning();

    for (std::size_t i{0U}; i < Enocean::k_max_devices; ++i) {
        // Use a unique switch address for each device.
        std::array<std::uint8_t, 6U> addr{
            static_cast<std::uint8_t>(i), 0x00U, 0x00U, 0x00U, 0xDAU, 0x03U};
        const auto cp{
            make_commissioning_payload(static_cast<std::uint32_t>(i + 1U))};
        const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                                std::span<const std::uint8_t>(cp))};
        driver_->process_advertisement(
            std::span<const std::uint8_t>(addr), 0x01U, 0x03U, -50,
            std::span<const std::uint8_t>(adv.data(), adv.size()));
    }

    EXPECT_EQ(driver_->device_count(), Enocean::k_max_devices);

    // One more should be silently rejected.
    std::array<std::uint8_t, 6U> extra_addr{0xFFU, 0xFFU, 0xFFU,
                                            0xFFU, 0xDAU, 0x03U};
    const auto cp{make_commissioning_payload(99U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    reset_captures();
    driver_->process_advertisement(
        std::span<const std::uint8_t>(extra_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_EQ(driver_->device_count(), Enocean::k_max_devices);
    EXPECT_FALSE(g_commissioned.called);
}

// ===========================================================================
// A. Commissioning boundary and format
// ===========================================================================

TEST_F(EnoceanDriverTest, CommissioningPayload21BytesRejected) {
    // 21-byte payload → 25-byte full AD record <
    // k_commissioning_payload_min(26) — must be silently ignored.
    driver_->enable_commissioning();
    const auto cp{make_commissioning_payload_sized(1U, 21U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 0U);
    EXPECT_FALSE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, CommissioningPayload22BytesAccepted) {
    // 22-byte payload → 26-byte full AD record =
    // k_commissioning_payload_min(26) — must succeed.
    driver_->enable_commissioning();
    const auto cp{make_commissioning_payload_sized(1U, 22U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 1U);
    EXPECT_TRUE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, Stm550bCommissioningWithMarkerAccepted) {
    // STM-550B format: seq(4)+0x3E(1)+key(16)+addr(6) = 27 bytes.
    // Key starts at offset 5 (driver must detect 0x3E at offset 4).
    constexpr std::array<std::uint8_t, Enocean::k_key_len> key{
        0xA0U, 0xA1U, 0xA2U, 0xA3U, 0xA4U, 0xA5U, 0xA6U, 0xA7U,
        0xA8U, 0xA9U, 0xAAU, 0xABU, 0xACU, 0xADU, 0xAEU, 0xAFU};
    const auto cp{make_stm550b_commissioning(10U, key)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->enable_commissioning();
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_stm550b_addr), 0x01U, 0x03U, -55,
        std::span<const std::uint8_t>(adv.data(), adv.size()));
    EXPECT_EQ(driver_->device_count(), 1U);
    EXPECT_TRUE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, CommissioningKeyCorrectlyExtracted) {
    // Verify the key stored during commissioning is passed verbatim to crypto
    // during the next data packet.  Uses the fixture's crypto_ (MockCrypto
    // records the last key passed to aes_ccm_decrypt_impl).
    constexpr std::array<std::uint8_t, Enocean::k_key_len> expected_key{
        0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U,
        0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU, 0x00U};

    // Commission with a known specific key.
    const auto cp{make_ptm216b_commissioning(1U, expected_key)};
    const auto adv_c{make_adv(Enocean::k_enocean_manufacturer_id,
                              std::span<const std::uint8_t>(cp))};
    driver_->enable_commissioning();
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv_c.data(), adv_c.size()));
    ASSERT_EQ(driver_->device_count(), 1U);

    // Send a switch data packet; crypto must receive the commissioned key.
    crypto_.reset_recording();
    driver_->disable_commissioning();
    constexpr std::uint8_t state{0x01U | (Enocean::k_switch_oa << 1U)};
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, state);

    EXPECT_TRUE(crypto_.was_called);
    EXPECT_EQ(crypto_.last_key, expected_key);
}

TEST_F(EnoceanDriverTest, NonceBuildingCorrect) {
    // Nonce layout per EnOcean spec: addr[0..5] + seq_LE[0..3] + pad[0..2]=0.
    // Uses fixture's crypto_ (MockCrypto) which records last_nonce.
    constexpr std::array<std::uint8_t, Enocean::k_key_len> key{};
    const auto cp{make_ptm216b_commissioning(1U, key)};
    const auto adv_c{make_adv(Enocean::k_enocean_manufacturer_id,
                              std::span<const std::uint8_t>(cp))};
    driver_->enable_commissioning();
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv_c.data(), adv_c.size()));
    ASSERT_EQ(driver_->device_count(), 1U);

    crypto_.reset_recording();
    driver_->disable_commissioning();
    constexpr std::uint32_t test_seq{0x00000005U};
    send_switch(std::span<const std::uint8_t>(k_switch_addr), test_seq, 0x01U);

    ASSERT_TRUE(crypto_.was_called);
    // nonce[0..5] = BLE addr (LE: addr[0]=LSB)
    for (std::size_t i{0U}; i < Enocean::k_addr_len; ++i) {
        EXPECT_EQ(crypto_.last_nonce[i], k_switch_addr[i])
            << "nonce addr byte " << i;
    }
    // nonce[6..9] = seq LE
    EXPECT_EQ(crypto_.last_nonce[6U],
              static_cast<std::uint8_t>(test_seq & 0xFFU));
    EXPECT_EQ(crypto_.last_nonce[7U], 0x00U);
    EXPECT_EQ(crypto_.last_nonce[8U], 0x00U);
    EXPECT_EQ(crypto_.last_nonce[9U], 0x00U);
    // nonce[10..12] = zero padding
    EXPECT_EQ(crypto_.last_nonce[10U], 0x00U);
    EXPECT_EQ(crypto_.last_nonce[11U], 0x00U);
    EXPECT_EQ(crypto_.last_nonce[12U], 0x00U);
}

TEST_F(EnoceanDriverTest, RecommissionSameDeviceUpdatesSeq) {
    // Recommission the same address with a higher seq → accepted (seq updated,
    // device count stays at 1).
    commission_switch(5U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();

    driver_->enable_commissioning();
    const auto cp{make_commissioning_payload(10U)};  // seq 10 > 5
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_EQ(driver_->device_count(), 1U);  // no duplicate added
    // Now a data packet with seq=11 must succeed (seq was updated to 10).
    reset_captures();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 11U, 0x01U);
    EXPECT_TRUE(g_button.called);
}

TEST_F(EnoceanDriverTest, RecommissionSameDeviceSameSeqRejected) {
    // Recommission with the same sequence number → replay, device table
    // unchanged.
    commission_switch(5U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();

    driver_->enable_commissioning();
    const auto cp{make_commissioning_payload(5U)};  // same seq
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -50,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    // Device count should still be 1, and seq should NOT have advanced,
    // so seq=6 must still work (original device entry intact).
    EXPECT_EQ(driver_->device_count(), 1U);
    reset_captures();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 6U, 0x01U);
    EXPECT_TRUE(g_button.called);
}

// ===========================================================================
// B. Device type detection and routing
// ===========================================================================

TEST_F(EnoceanDriverTest, Ptm215bAddressDetectedAsSwitch) {
    // PTM-215B: addr[5]=0x03, addr[4]=0xDA → DeviceType::Switch →
    // handle_switch.
    commission_switch(1U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, 0x01U);
    EXPECT_TRUE(g_button.called);
    EXPECT_FALSE(g_sensor.called);
}

TEST_F(EnoceanDriverTest, Ptm216bAddressDetectedAsSwitch) {
    // PTM-216B: addr[5]=0xE2, addr[4]=0x15 → DeviceType::Switch →
    // handle_switch.
    commission_device(std::span<const std::uint8_t>(k_switch_ptm216b_addr), 1U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_ptm216b_addr), 2U,
                0x01U);
    EXPECT_TRUE(g_button.called);
    EXPECT_FALSE(g_sensor.called);
}

TEST_F(EnoceanDriverTest, Stm550bAddressDetectedAsSensor) {
    // STM-550B: addr[5]=0xE5 → DeviceType::Sensor → handle_sensor.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();
    driver_->disable_commissioning();

    // Send minimal sensor packet: seq(4) + occupancy TLV(2) + tag(4) = 10
    // bytes. Occupancy TLV: header=0x20 (size_bits=00=1B, type=0x20),
    // value=0x02 (occupied).
    const std::array<std::uint8_t, 2U> tlv{0x20U, 0x02U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));
    EXPECT_TRUE(g_sensor.called);
    EXPECT_FALSE(g_button.called);
    EXPECT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_TRUE(*g_sensor.data.occupancy);
}

TEST_F(EnoceanDriverTest, UnknownAddressRoutedAsSensor) {
    // Unknown device type (neither switch nor sensor address) → fallback to
    // sensor.
    commission_device(std::span<const std::uint8_t>(k_unknown_type_addr), 1U);
    ASSERT_EQ(driver_->device_count(), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x20U,
                                           0x00U};  // occupancy = not occupied
    send_sensor(std::span<const std::uint8_t>(k_unknown_type_addr), 2U,
                std::span<const std::uint8_t>(tlv));
    EXPECT_TRUE(g_sensor.called);
    EXPECT_FALSE(g_button.called);
}

// ===========================================================================
// C. PTM-216B switch button encoding (Figure 16, section 4.6.2)
// ===========================================================================

TEST_F(EnoceanDriverTest, SwitchAllFourButtonsIndividuallyPress) {
    // Each button individually: bit0=1 (Press), one of bits1-4 set.
    struct Case {
        std::uint8_t state;
        std::uint8_t expected_changed;
    };
    const Case cases[]{
        {static_cast<std::uint8_t>(0x01U | (Enocean::k_switch_oa << 1U)),
         Enocean::k_switch_oa},
        {static_cast<std::uint8_t>(0x01U | (Enocean::k_switch_ia << 1U)),
         Enocean::k_switch_ia},
        {static_cast<std::uint8_t>(0x01U | (Enocean::k_switch_ob << 1U)),
         Enocean::k_switch_ob},
        {static_cast<std::uint8_t>(0x01U | (Enocean::k_switch_ib << 1U)),
         Enocean::k_switch_ib},
    };
    for (const auto& c : cases) {
        commission_switch(1U);
        reset_captures();
        driver_->disable_commissioning();
        send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, c.state);
        EXPECT_TRUE(g_button.called);
        EXPECT_EQ(g_button.action, Enocean::ButtonAction::Press);
        EXPECT_EQ(g_button.changed, c.expected_changed);
        // Re-init driver for next iteration.
        driver_ = std::make_unique<Enocean::EnoceanDriver<MockCrypto>>(
            crypto_, callbacks_);
        (void)driver_->init();
    }
}

TEST_F(EnoceanDriverTest, SwitchAllFourButtonsIndividuallyRelease) {
    // Each button individually: bit0=0 (Release), one of bits1-4 set.
    struct Case {
        std::uint8_t state;
        std::uint8_t expected_changed;
    };
    const Case cases[]{
        {static_cast<std::uint8_t>(Enocean::k_switch_oa << 1U),
         Enocean::k_switch_oa},
        {static_cast<std::uint8_t>(Enocean::k_switch_ia << 1U),
         Enocean::k_switch_ia},
        {static_cast<std::uint8_t>(Enocean::k_switch_ob << 1U),
         Enocean::k_switch_ob},
        {static_cast<std::uint8_t>(Enocean::k_switch_ib << 1U),
         Enocean::k_switch_ib},
    };
    for (const auto& c : cases) {
        commission_switch(1U);
        reset_captures();
        driver_->disable_commissioning();
        send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, c.state);
        EXPECT_TRUE(g_button.called);
        EXPECT_EQ(g_button.action, Enocean::ButtonAction::Release);
        EXPECT_EQ(g_button.changed, c.expected_changed);
        driver_ = std::make_unique<Enocean::EnoceanDriver<MockCrypto>>(
            crypto_, callbacks_);
        (void)driver_->init();
    }
}

TEST_F(EnoceanDriverTest, SwitchMultipleButtonsSimultaneous) {
    // All four buttons pressed simultaneously: bits1-4 all set, bit0=1 (Press).
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    constexpr std::uint8_t all_buttons{
        Enocean::k_switch_oa | Enocean::k_switch_ia | Enocean::k_switch_ob |
        Enocean::k_switch_ib};
    constexpr std::uint8_t state{0x01U |
                                 static_cast<std::uint8_t>(all_buttons << 1U)};
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, state);

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Press);
    EXPECT_EQ(g_button.changed, all_buttons);
}

TEST_F(EnoceanDriverTest, SwitchPressWithNoBitsYieldsZeroChanged) {
    // state=0x01 (bit0=Press, no button bits) → action=Press, changed=0.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, 0x01U);
    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Press);
    EXPECT_EQ(g_button.changed, 0x00U);
}

TEST_F(EnoceanDriverTest, SwitchReleaseWithNoBitsYieldsZeroChanged) {
    // state=0x00 (bit0=Release, no button bits) → action=Release, changed=0.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, 0x00U);
    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Release);
    EXPECT_EQ(g_button.changed, 0x00U);
}

TEST_F(EnoceanDriverTest, SwitchReservedBitsInStateRejected) {
    // bits[7:5] are reserved (shall be 0). Packets with reserved bits set
    // are rejected (per NCS SDK enocean.c behavior).
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    // bits[7:5]=111, bit4..1=OA, bit0=Press
    constexpr std::uint8_t state{
        0xE0U | 0x01U | static_cast<std::uint8_t>(Enocean::k_switch_oa << 1U)};
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, state);
    EXPECT_FALSE(g_button.called);
}

// ===========================================================================
// D. Switch optional data (PTM-216B, 0/1/2/4 bytes of optional user data)
// ===========================================================================

TEST_F(EnoceanDriverTest, SwitchOneByteOptionalData) {
    // 9 + 1 optional byte = 10 bytes total payload.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 1U> opt{0xABU};
    constexpr std::uint8_t state{0x01U | (Enocean::k_switch_oa << 1U)};
    const auto sp{make_switch_payload_with_opt(
        2U, state, std::span<const std::uint8_t>(opt))};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.action, Enocean::ButtonAction::Press);
    EXPECT_EQ(g_button.opt_len, 1U);
}

TEST_F(EnoceanDriverTest, SwitchTwoByteOptionalData) {
    // 9 + 2 optional bytes = 11 bytes total.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> opt{0x01U, 0x02U};
    constexpr std::uint8_t state{0x01U | (Enocean::k_switch_ob << 1U)};
    const auto sp{make_switch_payload_with_opt(
        2U, state, std::span<const std::uint8_t>(opt))};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.opt_len, 2U);
}

TEST_F(EnoceanDriverTest, SwitchFourByteOptionalData) {
    // 9 + 4 optional bytes = 13 bytes total (maximum per spec).
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 4U> opt{0xCAU, 0xFEU, 0xBAU, 0xBEU};
    constexpr std::uint8_t state{0x01U | (Enocean::k_switch_ib << 1U)};
    const auto sp{make_switch_payload_with_opt(
        2U, state, std::span<const std::uint8_t>(opt))};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(sp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_TRUE(g_button.called);
    EXPECT_EQ(g_button.opt_len, 4U);
}

// ===========================================================================
// E. Sensor TLV field parsing (STM-550B Sensor Protocol Specification)
// ===========================================================================

TEST_F(EnoceanDriverTest, SensorTemperaturePositive) {
    // Type 0x00, size 2B (size_bits=01 → header=0x40).
    // 23.50 °C = 2350 = 0x092E → LE bytes [0x2E, 0x09].
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 3U> tlv{0x40U, 0x2EU, 0x09U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.temperature_cdeg.has_value());
    EXPECT_EQ(*g_sensor.data.temperature_cdeg, static_cast<std::int16_t>(2350));
}

TEST_F(EnoceanDriverTest, SensorTemperatureNegative) {
    // -5.00 °C = -500 as int16_t = 0xFE0C (two's complement) → LE [0x0C, 0xFE].
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::int16_t val{-500};
    const auto raw{static_cast<std::uint16_t>(val)};
    const std::array<std::uint8_t, 3U> tlv{
        0x40U, static_cast<std::uint8_t>(raw & 0xFFU),
        static_cast<std::uint8_t>((raw >> 8U) & 0xFFU)};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.temperature_cdeg.has_value());
    EXPECT_EQ(*g_sensor.data.temperature_cdeg, static_cast<std::int16_t>(-500));
}

TEST_F(EnoceanDriverTest, SensorBatteryVoltage) {
    // Type 0x01, size 2B (header=0x41). Driver: battery_voltage = raw/2.
    // 3000 mV → raw=6000=0x1770 → LE [0x70, 0x17].
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 3U> tlv{0x41U, 0x70U, 0x17U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.battery_voltage.has_value());
    EXPECT_EQ(*g_sensor.data.battery_voltage, 3000U);
}

TEST_F(EnoceanDriverTest, SensorEnergyLevel) {
    // Type 0x02, size 1B (size_bits=00 → header=0x02). Driver: energy_lvl =
    // raw/2. 50 % → raw=100=0x64.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x02U, 0x64U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.energy_lvl.has_value());
    EXPECT_EQ(*g_sensor.data.energy_lvl, 50U);
}

TEST_F(EnoceanDriverTest, SensorLightSolarCell) {
    // Type 0x04, size 2B (header=0x44). raw=lux directly.
    // 500 lux = 0x01F4 → LE [0xF4, 0x01].
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 3U> tlv{0x44U, 0xF4U, 0x01U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.light_solar_cell.has_value());
    EXPECT_EQ(*g_sensor.data.light_solar_cell, 500U);
}

TEST_F(EnoceanDriverTest, SensorLightSensor) {
    // Type 0x05, size 2B (header=0x45). 1000 lux = 0x03E8 → LE [0xE8, 0x03].
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 3U> tlv{0x45U, 0xE8U, 0x03U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.light_sensor.has_value());
    EXPECT_EQ(*g_sensor.data.light_sensor, 1000U);
}

TEST_F(EnoceanDriverTest, SensorHumidity) {
    // Type 0x06, size 1B (header=0x06). Driver: humidity = (raw & 0xFF) / 2.
    // 60 %RH → raw=120=0x78.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x06U, 0x78U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.humidity.has_value());
    EXPECT_EQ(*g_sensor.data.humidity, 60U);
}

TEST_F(EnoceanDriverTest, SensorOccupancyOccupied) {
    // Type 0x20, size 1B (size_bits=00 → header=0x20). 0x02 = occupied.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x20U, 0x02U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_TRUE(*g_sensor.data.occupancy);
}

TEST_F(EnoceanDriverTest, SensorOccupancyVacant) {
    // Value=0 → not occupied.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x20U, 0x00U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_FALSE(*g_sensor.data.occupancy);
}

TEST_F(EnoceanDriverTest, SensorContactOpen) {
    // Type 0x23, size 1B (header=0x23). 0x01=open.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x23U, 0x01U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.contact.has_value());
    EXPECT_FALSE(*g_sensor.data.contact);  // open = false
}

TEST_F(EnoceanDriverTest, SensorContactClosed) {
    // 0x02=closed.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x23U, 0x02U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.contact.has_value());
    EXPECT_TRUE(*g_sensor.data.contact);
}

TEST_F(EnoceanDriverTest, SensorAccelerationManualExample) {
    // STM-550B Appendix B.1.3 reference example.
    // Acceleration type 0x0A, 4B (size_bits=10 → header=0x8A).
    // Wire bytes (LE): F7 91 E6 5E → uint32 = 0x5EE691F7.
    // bits[31:30]=01=status1, bits[29:20]=0x1EE=494→x=-18,
    // bits[19:10]=0x1A4=420→y=-92, bits[9:0]=0x1F7=503→z=-9.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 5U> tlv{0x8AU, 0xF7U, 0x91U, 0xE6U, 0x5EU};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.accel_status.has_value());
    EXPECT_EQ(*g_sensor.data.accel_status, 1U);  // status=01
    ASSERT_TRUE(g_sensor.data.accel_x_cg.has_value());
    EXPECT_EQ(*g_sensor.data.accel_x_cg, static_cast<std::int16_t>(-18));
    ASSERT_TRUE(g_sensor.data.accel_y_cg.has_value());
    EXPECT_EQ(*g_sensor.data.accel_y_cg, static_cast<std::int16_t>(-92));
    ASSERT_TRUE(g_sensor.data.accel_z_cg.has_value());
    EXPECT_EQ(*g_sensor.data.accel_z_cg, static_cast<std::int16_t>(-9));
}

TEST_F(EnoceanDriverTest, SensorAccelerationStatusBits) {
    // Verify status=0 (unequipped) and status=3 (disabled) from the bit field.
    // status=0: bits[31:30]=00 → value with upper two bits clear.
    // Use value=0x00000000 → status=0, x=y=z=-512? No: (0-512)=-512 each.
    // Use value with status=3: 0xC0000000 → bits[31:30]=11, rest 0.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    // status=3 (disabled): uint32=0xC0000000 → LE bytes [0x00, 0x00, 0x00,
    // 0xC0].
    const std::array<std::uint8_t, 5U> tlv{0x8AU, 0x00U, 0x00U, 0x00U, 0xC0U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.accel_status.has_value());
    EXPECT_EQ(*g_sensor.data.accel_status, 3U);
}

TEST_F(EnoceanDriverTest, SensorMultipleTlvFields) {
    // Combined packet: temperature + humidity + occupancy in one payload.
    // Temp: 0x40 0x2E 0x09  (23.50°C)
    // Humidity: 0x06 0x78   (60%)
    // Occupancy: 0x20 0x02  (occupied)
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 7U> tlv{0x40U, 0x2EU,
                                           0x09U,         // temperature 23.50°C
                                           0x06U, 0x78U,  // humidity 60%
                                           0x20U, 0x02U};  // occupancy occupied
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.temperature_cdeg.has_value());
    EXPECT_EQ(*g_sensor.data.temperature_cdeg, 2350);
    ASSERT_TRUE(g_sensor.data.humidity.has_value());
    EXPECT_EQ(*g_sensor.data.humidity, 60U);
    ASSERT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_TRUE(*g_sensor.data.occupancy);
}

// ===========================================================================
// F. Sensor TLV edge cases
// ===========================================================================

TEST_F(EnoceanDriverTest, SensorUnknownTlvTypeSkipped) {
    // Unknown type (not in the switch-case) must be skipped; known fields after
    // it must still be parsed.
    // Unknown type 0x0B (size_bits=00=1B): header=0x0B, value=0xFF (skipped).
    // Then temperature: 0x40 0x2E 0x09.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 5U> tlv{
        0x0BU, 0xFFU,          // unknown 1-byte type → skipped
        0x40U, 0x2EU, 0x09U};  // temperature 23.50°C → parsed
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.temperature_cdeg.has_value());
    EXPECT_EQ(*g_sensor.data.temperature_cdeg, 2350);
}

TEST_F(EnoceanDriverTest, SensorVariableLengthTlvSkipped) {
    // size_bits=11 (0xC0) → variable length: next byte is explicit length, then
    // skip. Variable header = 0xC0 | type, next byte = length. Use type=0x00
    // (size_bits=11) → header=0xC0, length=3, 3 garbage bytes. Then occupancy:
    // 0x20 0x02.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 7U> tlv{
        0xC0U, 0x03U, 0xAAU, 0xBBU, 0xCCU,  // variable-length: skip 3 bytes
        0x20U, 0x02U};                      // occupancy occupied
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_TRUE(*g_sensor.data.occupancy);
}

TEST_F(EnoceanDriverTest, SensorOptionalDataTlvSkipped) {
    // type=0x3C (OPTIONAL_DATA) triggers variable-length skip regardless of
    // size_bits. header = 0x3C, next byte = length=2, 2 bytes payload. Then
    // humidity: 0x06 0x78.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 6U> tlv{0x3CU, 0x02U,
                                           0xAAU, 0xBBU,  // optional data: skip
                                           0x06U, 0x78U};  // humidity 60%
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.humidity.has_value());
    EXPECT_EQ(*g_sensor.data.humidity, 60U);
}

TEST_F(EnoceanDriverTest, SensorCommissioningBlockInDataSkipped) {
    // type=0x3E (COMMISSIONING) in a data telegram → skip 22 bytes.
    // Build a TLV block: header=0x3E then 22 zero bytes, then occupancy.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    std::vector<std::uint8_t> tlv;
    tlv.push_back(0x3EU);  // commissioning block header
    for (std::size_t i{0U}; i < 22U; ++i) {
        tlv.push_back(0x00U);
    }  // 22 bytes
    tlv.push_back(0x20U);
    tlv.push_back(0x02U);  // occupancy: occupied
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    ASSERT_TRUE(g_sensor.called);
    ASSERT_TRUE(g_sensor.data.occupancy.has_value());
    EXPECT_TRUE(*g_sensor.data.occupancy);
}

TEST_F(EnoceanDriverTest, SensorTruncatedTlvSafe) {
    // A 2-byte TLV where only the header byte is present (no value bytes).
    // Driver must not read past end; remaining fields are silently dropped.
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    reset_captures();
    driver_->disable_commissioning();

    // Truncated 2-byte temperature (header only, no value bytes).
    const std::array<std::uint8_t, 1U> tlv{0x40U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));

    // Must not crash; sensor callback still fired, field simply not set.
    EXPECT_TRUE(g_sensor.called);
    EXPECT_FALSE(g_sensor.data.temperature_cdeg.has_value());
}

// ===========================================================================
// G. Sequence number enforcement and state updates
// ===========================================================================

TEST_F(EnoceanDriverTest, SwitchSeqEqualStoredRejected) {
    // After commissioning with seq=5, sending data with seq=5 (not strictly >)
    // fails.
    commission_switch(5U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 5U, 0x01U);
    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, SwitchSeqLessThanStoredRejected) {
    commission_switch(10U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 9U, 0x01U);
    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, SwitchSeqStrictlyGreaterAccepted) {
    commission_switch(10U);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 11U, 0x01U);
    EXPECT_TRUE(g_button.called);
}

TEST_F(EnoceanDriverTest, SwitchSeqUpdatedAfterSuccess) {
    // After a successful event with seq=5, seq=5 again must be rejected.
    commission_switch(1U);
    driver_->disable_commissioning();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 5U, 0x01U);
    ASSERT_TRUE(g_button.called);

    reset_captures();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 5U, 0x01U);
    EXPECT_FALSE(g_button.called);  // replay
}

TEST_F(EnoceanDriverTest, AuthFailureDoesNotUpdateSeq) {
    // Auth failure → seq must NOT advance; a subsequent valid packet must
    // succeed.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    crypto_.auth_result = false;
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 5U, 0x01U);
    EXPECT_FALSE(g_button.called);  // auth failed

    // Restore auth; seq=5 must still be accepted (was not committed).
    crypto_.auth_result = true;
    reset_captures();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 5U, 0x01U);
    EXPECT_TRUE(g_button.called);
}

TEST_F(EnoceanDriverTest, SensorReplayRejected) {
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 5U);
    reset_captures();
    driver_->disable_commissioning();

    const std::array<std::uint8_t, 2U> tlv{0x20U, 0x01U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 3U,
                std::span<const std::uint8_t>(tlv));  // seq=3 < 5
    EXPECT_FALSE(g_sensor.called);
}

TEST_F(EnoceanDriverTest, SwitchSeqUINT32MAXThenZeroRejected) {
    // Commission with seq=UINT32_MAX. Then seq=0 must be rejected because 0 <
    // UINT32_MAX.
    commission_switch(0xFFFFFFFFU);
    reset_captures();
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 0U, 0x01U);
    EXPECT_FALSE(g_button.called);
}

// ===========================================================================
// H. Multiple devices independence
// ===========================================================================

TEST_F(EnoceanDriverTest, TwoSwitchesIndependent) {
    // Events from addr1 must not trigger the callback attributed to addr2.
    constexpr std::array<std::uint8_t, 6U> addr1{0x11U, 0x00U, 0x00U,
                                                 0x00U, 0xDAU, 0x03U};
    constexpr std::array<std::uint8_t, 6U> addr2{0x22U, 0x00U, 0x00U,
                                                 0x00U, 0xDAU, 0x03U};

    driver_->enable_commissioning();
    commission_device(std::span<const std::uint8_t>(addr1), 1U);
    commission_device(std::span<const std::uint8_t>(addr2), 1U);
    ASSERT_EQ(driver_->device_count(), 2U);
    driver_->disable_commissioning();

    // Send from addr1 only → callback fires once.
    reset_captures();
    send_switch(std::span<const std::uint8_t>(addr1), 2U, 0x01U);
    EXPECT_TRUE(g_button.called);

    // Send from addr2 → callback fires again (separate device state).
    reset_captures();
    send_switch(std::span<const std::uint8_t>(addr2), 2U, 0x01U);
    EXPECT_TRUE(g_button.called);

    // Replay from addr1 (seq=2 again) → rejected.
    reset_captures();
    send_switch(std::span<const std::uint8_t>(addr1), 2U, 0x01U);
    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, SwitchAndSensorBothRouted) {
    // One switch and one sensor commissioned; correct handler is called for
    // each.
    commission_device(std::span<const std::uint8_t>(k_switch_addr), 1U);
    commission_device(std::span<const std::uint8_t>(k_stm550b_addr), 1U);
    ASSERT_EQ(driver_->device_count(), 2U);
    driver_->disable_commissioning();

    // Switch event → button callback only.
    reset_captures();
    send_switch(std::span<const std::uint8_t>(k_switch_addr), 2U, 0x01U);
    EXPECT_TRUE(g_button.called);
    EXPECT_FALSE(g_sensor.called);

    // Sensor event → sensor callback only.
    reset_captures();
    const std::array<std::uint8_t, 2U> tlv{0x20U, 0x01U};
    send_sensor(std::span<const std::uint8_t>(k_stm550b_addr), 2U,
                std::span<const std::uint8_t>(tlv));
    EXPECT_TRUE(g_sensor.called);
    EXPECT_FALSE(g_button.called);
}

// ===========================================================================
// I. General edge cases
// ===========================================================================

TEST_F(EnoceanDriverTest, DataFromUncommissionedDeviceIgnored) {
    // A data packet from an address that was never commissioned must be
    // ignored.
    driver_->disable_commissioning();

    send_switch(std::span<const std::uint8_t>(k_switch_addr), 1U, 0x01U);
    EXPECT_FALSE(g_button.called);
    EXPECT_EQ(driver_->device_count(), 0U);
}

TEST_F(EnoceanDriverTest, PayloadBelow9BytesIgnored) {
    // 8-byte payload < k_data_payload_min(9) → silently dropped even for known
    // device.
    commission_switch(1U);
    reset_captures();
    driver_->disable_commissioning();

    // Build 8-byte payload manually (seq(4)+state(1)+tag(3) — one tag byte
    // short).
    const std::array<std::uint8_t, 8U> short_p{
        0x02U, 0x00U, 0x00U, 0x00U,  // seq=2
        0x01U,                       // state
        0x00U, 0x00U, 0x00U};        // only 3 tag bytes
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(short_p))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr), 0x01U, 0x03U, -60,
        std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_FALSE(g_button.called);
}

TEST_F(EnoceanDriverTest, PublicAddressAdvertisementIgnored) {
    // addr_type=0x00 (public) must be rejected (driver requires random=0x01).
    driver_->enable_commissioning();
    const auto cp{make_commissioning_payload(1U)};
    const auto adv{make_adv(Enocean::k_enocean_manufacturer_id,
                            std::span<const std::uint8_t>(cp))};
    driver_->process_advertisement(
        std::span<const std::uint8_t>(k_switch_addr),
        0x00U,  // public address — must be rejected
        0x03U, -50, std::span<const std::uint8_t>(adv.data(), adv.size()));

    EXPECT_EQ(driver_->device_count(), 0U);
    EXPECT_FALSE(g_commissioned.called);
}

TEST_F(EnoceanDriverTest, EmptyAdvertisementPayloadIgnored) {
    // Zero-length advertisement data → no crash, nothing happens.
    driver_->enable_commissioning();
    const std::vector<std::uint8_t> empty{};
    driver_->process_advertisement(std::span<const std::uint8_t>(k_switch_addr),
                                   0x01U, 0x03U, -50,
                                   std::span<const std::uint8_t>(empty));
    EXPECT_EQ(driver_->device_count(), 0U);
}

TEST_F(EnoceanDriverTest, CommissioningModeToggles) {
    EXPECT_FALSE(driver_->commissioning_enabled());
    driver_->enable_commissioning();
    EXPECT_TRUE(driver_->commissioning_enabled());
    driver_->disable_commissioning();
    EXPECT_FALSE(driver_->commissioning_enabled());
}

/// @endcond
