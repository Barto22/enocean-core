#include "AppEntry.hpp"

#include <cstdint>
#include <logging/Logger.hpp>
#include <variant>

#include "Ble.hpp"
#include "Crypto.hpp"
#include "Sleep.hpp"
#include "enocean/EnoceanDriver.hpp"
#include "enocean/EnoceanTypes.hpp"

namespace {

void on_button(const Enocean::EnoceanDevice& device,
               Enocean::ButtonAction action, std::uint8_t changed,
               [[maybe_unused]] const std::uint8_t* opt_data,
               [[maybe_unused]] std::size_t opt_len) noexcept {
    const auto& a{device.ble_addr.addr};
    LOGGER_NOTICE(
        "EnOcean button %s | addr %02X:%02X:%02X:%02X:%02X:%02X"
        " | buttons 0x%02X",
        (action == Enocean::ButtonAction::Press) ? "PRESS" : "RELEASE", a[5U],
        a[4U], a[3U], a[2U], a[1U], a[0U], changed);
}

void on_sensor(const Enocean::EnoceanDevice& device,
               const Enocean::SensorData& data,
               [[maybe_unused]] const std::uint8_t* opt_data,
               [[maybe_unused]] std::size_t opt_len) noexcept {
    const auto& a{device.ble_addr.addr};
    LOGGER_NOTICE("EnOcean sensor | addr %02X:%02X:%02X:%02X:%02X:%02X", a[5U],
                  a[4U], a[3U], a[2U], a[1U], a[0U]);

    if (data.temperature_cdeg.has_value()) {
        const std::int16_t t{*data.temperature_cdeg};
        const std::int16_t ta{static_cast<std::int16_t>(t < 0 ? -t : t)};
        LOGGER_NOTICE("  Temp (°C)    : %c%d.%02d", t < 0 ? '-' : '+',
                      static_cast<int>(ta / 100), static_cast<int>(ta % 100));
    }
    if (data.humidity.has_value()) {
        LOGGER_NOTICE("  Humidity (%%) : %u", *data.humidity);
    }
    if (data.occupancy.has_value()) {
        LOGGER_NOTICE("  Occupancy    : %s", *data.occupancy ? "yes" : "no");
    }
    if (data.light_sensor.has_value()) {
        LOGGER_NOTICE("  Light (lx)   : %u", *data.light_sensor);
    }
    if (data.light_solar_cell.has_value()) {
        LOGGER_NOTICE("  Solar (lx)   : %u", *data.light_solar_cell);
    }
    if (data.energy_lvl.has_value()) {
        LOGGER_NOTICE("  Energy (%%)   : %u", *data.energy_lvl);
    }
    if (data.battery_voltage.has_value()) {
        LOGGER_NOTICE("  Battery (mV) : %u", *data.battery_voltage);
    }
    if (data.contact.has_value()) {
        LOGGER_NOTICE("  Contact      : %s", *data.contact ? "closed" : "open");
    }
    if (data.accel_status.has_value()) {
        LOGGER_NOTICE("  Accel status : %u", *data.accel_status);
        if (*data.accel_status != 3U) {
            LOGGER_NOTICE("  Accel (0.01g): X=%d Y=%d Z=%d",
                          static_cast<int>(*data.accel_x_cg),
                          static_cast<int>(*data.accel_y_cg),
                          static_cast<int>(*data.accel_z_cg));
        }
    }
}

void on_commissioned(const Enocean::EnoceanDevice& device) noexcept {
    const auto& a{device.ble_addr.addr};
    LOGGER_NOTICE("EnOcean device COMMISSIONED: %02X:%02X:%02X:%02X:%02X:%02X",
                  a[5U], a[4U], a[3U], a[2U], a[1U], a[0U]);
}

void on_decommissioned(const Enocean::EnoceanDevice& device) noexcept {
    const auto& a{device.ble_addr.addr};
    LOGGER_NOTICE(
        "EnOcean device DECOMMISSIONED: %02X:%02X:%02X:%02X:%02X:%02X", a[5U],
        a[4U], a[3U], a[2U], a[1U], a[0U]);
}

void on_loaded(const Enocean::EnoceanDevice& device) noexcept {
    const auto& a{device.ble_addr.addr};
    LOGGER_NOTICE("EnOcean device LOADED: %02X:%02X:%02X:%02X:%02X:%02X", a[5U],
                  a[4U], a[3U], a[2U], a[1U], a[0U]);
}

}  // namespace

namespace App {

void AppEntry::run_impl(
    [[maybe_unused]] PlatformCore::ThreadParamVariant& params) {
    LOGGER_NOTICE("Application starting...");

    Crypto::AesCcm crypto{};

    const Enocean::EnoceanCallbacks callbacks{
        .button = on_button,
        .sensor = on_sensor,
        .commissioned = on_commissioned,
        .decommissioned = on_decommissioned,
        .loaded = on_loaded,
    };

    Enocean::EnoceanDriver<Crypto::AesCcm> driver{crypto, callbacks};

    const auto init_result{driver.init()};
    if (!init_result) {
        LOGGER_ERROR("EnOcean driver init failed");
        return;
    }

    Ble::Scanner scanner{};

    const auto ble_init{scanner.init(
        &Enocean::EnoceanDriver<Crypto::AesCcm>::advertisement_cb, &driver)};
    if (!ble_init) {
        LOGGER_ERROR("BLE scanner init failed");
        return;
    }

    const auto scan_start{scanner.start_scan()};
    if (!scan_start) {
        LOGGER_ERROR("BLE scan start failed");
        return;
    }

    driver.enable_commissioning();

    LOGGER_NOTICE("EnOcean scanner ready - commissioning enabled.");

    PlatformCore::Sleep sleep{};
    while (true) {
        sleep.sleep_ms(1000);
    }
}

}  // namespace App
