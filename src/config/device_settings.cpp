#include "config/device_settings.h"

namespace kc868a6::config {
namespace {

const std::array<uint32_t, 5> baudRates{{9600, 19200, 38400, 57600, 115200}};
const std::array<uint32_t, 2> i2cClocks{{50000, 100000}};
const std::array<uint32_t, 2> outputPins{{25, 26}};
const std::array<SettingDescriptor, 33> schema{{
    {"sensorIntervalMs", "sensor_ms", &DeviceSettings::sensorIntervalMs, 500, 60000, {}},
    {"i2cClockHz", "i2c_hz", &DeviceSettings::i2cClockHz, 50000, 100000, i2cClocks},
    {"rs485Baud", "rs485_baud", &DeviceSettings::rs485Baud, 9600, 115200, baudRates},
    {"uart1Baud", "uart1_baud", &DeviceSettings::uart1Baud, 9600, 115200, baudRates},
    {"uart1Mode", "uart1_mode", &DeviceSettings::uart1Mode, 0, 2, {}},
    {"beeperFrequencyHz", "beep_hz", &DeviceSettings::beeperFrequencyHz, 250, 5000, {}},
    {"beeperDurationMs", "beep_ms", &DeviceSettings::beeperDurationMs, 50, 3000, {}},
    {"gpio32SensorType", "gpio32_type", &DeviceSettings::gpio32SensorType, 0, 3, {}},
    {"gpio33SensorType", "gpio33_type", &DeviceSettings::gpio33SensorType, 0, 3, {}},
    {"displayMode", "display_mode", &DeviceSettings::displayMode, 0, 5, {}},
    {"displayPageSeconds", "display_secs", &DeviceSettings::displayPageSeconds, 2, 60, {}},
    {"displayBrightness", "display_bright", &DeviceSettings::displayBrightness, 0, 255, {}},
    {"displayRotation", "display_rotate", &DeviceSettings::displayRotation, 0, 3, {}},
    {"displayTextSize", "display_size", &DeviceSettings::displayTextSize, 1, 2, {}},
    {"displayFieldMask", "display_fields", &DeviceSettings::displayFieldMask, 0, 63, {}},
    {"bluetoothMode", "bt_mode", &DeviceSettings::bluetoothMode, 0, 2, {}},
    {"beeperEnabled", "beep_on", &DeviceSettings::beeperEnabled, 0, 1, {}},
    {"displayEnabled", "display_on", &DeviceSettings::displayEnabled, 0, 1, {}},
    {"s2InputEnabled", "s2_input", &DeviceSettings::s2InputEnabled, 0, 1, {}},
    {"temperature1Enabled", "temp1_on", &DeviceSettings::temperature1Enabled, 0, 1, {}},
    {"temperature2Enabled", "temp2_on", &DeviceSettings::temperature2Enabled, 0, 1, {}},
    {"bme680Enabled", "bme_on", &DeviceSettings::bme680Enabled, 0, 1, {}},
    {"ens160Enabled", "ens_on", &DeviceSettings::ens160Enabled, 0, 1, {}},
    {"compassEnabled", "compass_on", &DeviceSettings::compassEnabled, 0, 1, {}},
    {"gpsEnabled", "gps_on", &DeviceSettings::gpsEnabled, 0, 1, {}},
    {"rtcEnabled", "rtc_on", &DeviceSettings::rtcEnabled, 0, 1, {}},
    {"pn532Enabled", "pn532_on", &DeviceSettings::pn532Enabled, 0, 1, {}},
    {"beeperPin", "beep_pin", &DeviceSettings::beeperPin, 25, 26, outputPins},
    {"fanEnabled", "fan_on", &DeviceSettings::fanEnabled, 0, 1, {}},
    {"fanPin", "fan_pin", &DeviceSettings::fanPin, 25, 26, outputPins},
    {"fanFrequencyHz", "fan_hz", &DeviceSettings::fanFrequencyHz, 100, 25000, {}},
    {"udpListenPort", "udp_port", &DeviceSettings::udpListenPort, 0, 65535, {}},
    {"blePresenceTimeoutSeconds", "ble_timeout", &DeviceSettings::blePresenceTimeoutSeconds, 5, 3600, {}},
}};

}  // namespace

core::List<SettingDescriptor> settingsSchema() { return schema; }

DeviceSettings defaultSettings(Uart1Mode mode) {
    DeviceSettings settings;
    settings.uart1Mode = static_cast<uint8_t>(mode);
    settings.uart1Baud = mode == Uart1Mode::Coprocessor ? 115200 : 9600;
    settings.gpsEnabled = mode == Uart1Mode::Gps;
    return settings;
}

void normalizeSettings(DeviceSettings &settings) {
    settings.temperature1Enabled = settings.gpio32SensorType != 0;
    settings.temperature2Enabled = settings.gpio33SensorType != 0;
}

core::Result validateSettings(const DeviceSettings &settings) {
    for (size_t index = 0; index < schema.size(); ++index) {
        const auto &field = schema[index];
        const uint32_t value = std::visit(
            [&settings](auto member) { return static_cast<uint32_t>(settings.*member); },
            field.member);
        if (value < field.minimum || value > field.maximum) {
            return {core::ErrorCode::InvalidArgument, field.name};
        }
        if (field.allowedValues.size == 0) continue;
        bool found = false;
        for (size_t allowed = 0; allowed < field.allowedValues.size; ++allowed) {
            if (field.allowedValues.data[allowed] == value) found = true;
        }
        if (!found) return {core::ErrorCode::InvalidArgument, field.name};
    }
    return {};
}

bool equalSettings(const DeviceSettings &left, const DeviceSettings &right) {
    for (const auto &field : schema) {
        if (!std::visit(
                [&left, &right](auto member) { return left.*member == right.*member; },
                field.member)) {
            return false;
        }
    }
    return true;
}

}  // namespace kc868a6::config
