#pragma once

#include "core/peripheral.h"

struct DeviceSettings {
    uint16_t sensorIntervalMs = 1000;
    uint32_t i2cClockHz = 100000;
    uint32_t rs485Baud = 9600;
    uint32_t uart1Baud = 9600;
    uint8_t uart1Mode = 0;
    uint16_t beeperFrequencyHz = 2000;
    uint16_t beeperDurationMs = 250;
    uint8_t gpio32SensorType = 2;
    uint8_t gpio33SensorType = 0;
    uint8_t displayMode = 0;
    uint8_t displayPageSeconds = 5;
    uint8_t displayBrightness = 160;
    uint8_t displayRotation = 0;
    uint8_t displayTextSize = 1;
    uint8_t displayFieldMask = 0x3F;
    uint8_t bluetoothMode = 1;
    bool beeperEnabled = false;
    bool displayEnabled = true;
    bool s2InputEnabled = false;
    bool temperature1Enabled = true;
    bool temperature2Enabled = false;
    bool bme680Enabled = true;
    bool ens160Enabled = true;
    bool compassEnabled = true;
    bool gpsEnabled = false;
    bool rtcEnabled = true;
    bool pn532Enabled = true;
    uint8_t beeperPin = 26;
    bool fanEnabled = false;
    uint8_t fanPin = 25;
    uint16_t fanFrequencyHz = 1000;
    uint16_t udpListenPort = 0;
    uint16_t blePresenceTimeoutSeconds = 60;
    uint8_t serial1Protocol = 0;
    uint8_t serial2Protocol = 0;
    uint8_t serial1DataBits = 8;
    uint8_t serial2DataBits = 8;
    uint8_t serial1Parity = 0;
    uint8_t serial2Parity = 0;
    uint8_t serial1StopBits = 1;
    uint8_t serial2StopBits = 1;
    uint8_t serial2Route = 0;
    bool ebusWritesEnabled = false;
};

namespace kc868a6::config {

enum class Uart1Mode : uint8_t { Serial = 0, Gps = 1, Coprocessor = 2 };

using SettingMember =
    std::variant<uint8_t DeviceSettings::*, uint16_t DeviceSettings::*,
                 uint32_t DeviceSettings::*, bool DeviceSettings::*>;

struct SettingDescriptor {
    const char *name;
    const char *legacyKey;
    SettingMember member;
    uint32_t minimum;
    uint32_t maximum;
    core::List<uint32_t> allowedValues;
};

core::List<SettingDescriptor> settingsSchema();
DeviceSettings defaultSettings(Uart1Mode mode);
void normalizeSettings(DeviceSettings &settings);
core::Result validateSettings(const DeviceSettings &settings);
bool equalSettings(const DeviceSettings &left, const DeviceSettings &right);

}  // namespace kc868a6::config
