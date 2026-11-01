#include "config/config_service.h"
#include "config/preferences_settings_store.h"
#include "config/settings_json.h"

#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>

using namespace kc868a6;

namespace {

core::Result patch(const char *json, const DeviceSettings &current,
                   DeviceSettings &candidate) {
    DynamicJsonDocument document(config::settingsJsonCapacity);
    assert(!deserializeJson(document, json));
    return config::readSettingsJson(document.as<JsonVariantConst>(), current, candidate);
}

DeviceSettings fixture() {
    DeviceSettings settings;
    settings.sensorIntervalMs = 60000;
    settings.i2cClockHz = 50000;
    settings.rs485Baud = 57600;
    settings.uart1Baud = 115200;
    settings.uart1Mode = 2;
    settings.beeperFrequencyHz = 5000;
    settings.beeperDurationMs = 3000;
    settings.gpio32SensorType = 0;
    settings.gpio33SensorType = 1;
    settings.displayMode = 4;
    settings.displayPageSeconds = 60;
    settings.displayBrightness = 255;
    settings.displayRotation = 3;
    settings.displayTextSize = 2;
    settings.displayFieldMask = 0;
    settings.bluetoothMode = 2;
    settings.beeperEnabled = true;
    settings.displayEnabled = false;
    settings.s2InputEnabled = true;
    settings.bme680Enabled = false;
    settings.ens160Enabled = false;
    settings.compassEnabled = false;
    settings.gpsEnabled = true;
    settings.rtcEnabled = false;
    settings.pn532Enabled = false;
    config::normalizeSettings(settings);
    return settings;
}

void seedLegacy(const DeviceSettings &settings) {
    const auto schema = config::settingsSchema();
    for (size_t index = 0; index < schema.size; ++index) {
        const auto &field = schema.data[index];
        std::visit(
            [&settings, &field](auto member) {
                using T = std::decay_t<decltype(settings.*member)>;
                if constexpr (std::is_same_v<T, bool>) {
                    Preferences::values[field.legacyKey] =
                        static_cast<uint8_t>(settings.*member);
                } else {
                    Preferences::values[field.legacyKey] = settings.*member;
                }
            },
            field.member);
    }
}

void testDefaultsAndSchema() {
    const auto serial = config::defaultSettings(config::Uart1Mode::Serial);
    const auto gps = config::defaultSettings(config::Uart1Mode::Gps);
    const auto peer = config::defaultSettings(config::Uart1Mode::Coprocessor);
    assert(serial.uart1Mode == 0 && serial.uart1Baud == 9600 && !serial.gpsEnabled);
    assert(gps.uart1Mode == 1 && gps.uart1Baud == 9600 && gps.gpsEnabled);
    assert(peer.uart1Mode == 2 && peer.uart1Baud == 115200 && !peer.gpsEnabled);
    assert(serial.temperature1Enabled && !serial.temperature2Enabled);
    assert(config::validateSettings(serial).ok());
    assert(config::validateSettings(gps).ok());
    assert(config::validateSettings(peer).ok());
    auto incompatibleGps = gps;
    incompatibleGps.serial1Parity = 1;
    assert(config::validateSettings(incompatibleGps).code == core::ErrorCode::ResourceConflict);
    const auto schema = config::settingsSchema();
    assert(schema.size == 43);
    const char *legacyKeys[] = {
        "sensor_ms", "i2c_hz", "rs485_baud", "uart1_baud", "uart1_mode",
        "beep_hz", "beep_ms", "gpio32_type", "gpio33_type", "display_mode",
        "display_secs", "display_bright", "display_rotate", "display_size",
        "display_fields", "bt_mode", "beep_on", "display_on", "s2_input",
        "temp1_on", "temp2_on", "bme_on", "ens_on", "compass_on", "gps_on",
        "rtc_on", "pn532_on", "beep_pin", "fan_on", "fan_pin", "fan_hz", "udp_port", "ble_timeout",
        "ser1_proto", "ser2_proto", "ser1_bits", "ser2_bits", "ser1_parity", "ser2_parity",
        "ser1_stop", "ser2_stop", "ser2_route", "ebus_write",
    };
    for (size_t index = 0; index < schema.size; ++index) {
        assert(std::string_view(schema.data[index].legacyKey) == legacyKeys[index]);
        assert(std::string(schema.data[index].legacyKey).length() <= 15);
        for (size_t previous = 0; previous < index; ++previous) {
            assert(std::string_view(schema.data[previous].name) != schema.data[index].name);
            assert(std::string_view(schema.data[previous].legacyKey) !=
                   schema.data[index].legacyKey);
        }
    }
}

void testJsonCompatibilityAndRoundTrip() {
    const auto settings = fixture();
    DynamicJsonDocument document(config::settingsJsonCapacity);
    assert(config::writeSettingsJson(document.to<JsonObject>(), settings).ok());
    assert(document.size() == 43 && !document.overflowed());
    assert(document["sensorIntervalMs"].as<uint32_t>() == 60000);
    assert(document["displayBrightness"].as<uint32_t>() == 255);
    assert(document["temperature2Enabled"].as<bool>());
    assert(!document.containsKey("schemaVersion"));
    std::string encoded;
    serializeJson(document, encoded);
    DeviceSettings decoded;
    assert(patch(encoded.c_str(), DeviceSettings{}, decoded).ok());
    assert(config::equalSettings(decoded, settings));
    document.clear();
    assert(config::writeSettingsJson(document.to<JsonObject>(), DeviceSettings{}).ok());
    encoded.clear();
    serializeJson(document, encoded);
    assert(encoded ==
        "{\"sensorIntervalMs\":1000,\"i2cClockHz\":100000,\"rs485Baud\":9600,"
        "\"uart1Baud\":9600,\"uart1Mode\":0,\"beeperFrequencyHz\":2000,"
        "\"beeperDurationMs\":250,\"gpio32SensorType\":2,\"gpio33SensorType\":0,"
        "\"displayMode\":0,\"displayPageSeconds\":5,\"displayBrightness\":160,"
        "\"displayRotation\":0,\"displayTextSize\":1,\"displayFieldMask\":63,"
        "\"bluetoothMode\":1,\"beeperEnabled\":false,\"displayEnabled\":true,"
        "\"s2InputEnabled\":false,\"temperature1Enabled\":true,"
        "\"temperature2Enabled\":false,\"bme680Enabled\":true,\"ens160Enabled\":true,"
        "\"compassEnabled\":true,\"gpsEnabled\":false,\"rtcEnabled\":true,"
        "\"pn532Enabled\":true,\"beeperPin\":26,\"fanEnabled\":false,"
        "\"fanPin\":25,\"fanFrequencyHz\":1000,\"udpListenPort\":0,"
        "\"blePresenceTimeoutSeconds\":60,\"serial1Protocol\":0,\"serial2Protocol\":0,"
        "\"serial1DataBits\":8,\"serial2DataBits\":8,\"serial1Parity\":0,"
        "\"serial2Parity\":0,\"serial1StopBits\":1,\"serial2StopBits\":1,"
        "\"serial2Route\":0,\"ebusWritesEnabled\":false}");
}

void testPartialPatchAndTypes() {
    const auto current = fixture();
    DeviceSettings candidate;
    assert(patch("{}", current, candidate).ok());
    assert(config::equalSettings(candidate, current));
    assert(patch("{\"gpio33SensorType\":0,\"temperature2Enabled\":true}",
                 current, candidate).ok());
    assert(!candidate.temperature2Enabled && candidate.gpio33SensorType == 0);
    assert(candidate.displayBrightness == current.displayBrightness);
    const auto before = candidate;
    const char *invalid[] = {
        "[]", "null", "123", "{\"unknown\":1}", "{\"sensorIntervalMs\":-1}",
        "{\"sensorIntervalMs\":\"1000\"}", "{\"sensorIntervalMs\":true}",
        "{\"sensorIntervalMs\":1000.5}", "{\"sensorIntervalMs\":4294967296}",
        "{\"displayBrightness\":256}", "{\"displayMode\":256}",
        "{\"beeperEnabled\":1}", "{\"beeperEnabled\":null}",
        "{\"displayTextSize\":0}", "{\"uart1Baud\":460800}",
        "{\"i2cClockHz\":75000}", "{\"gpio32SensorType\":4}",
        "{\"bluetoothMode\":3}", "{\"displayRotation\":4}",
        "{\"serial1Protocol\":2}", "{\"serial2DataBits\":9}",
        "{\"serial1Parity\":3}", "{\"serial2StopBits\":0}", "{\"serial2Route\":2}",
        "{\"serial1Protocol\":1}",
        "{\"sensorIntervalMs\":500,\"displayFieldMask\":64}",
    };
    for (const char *json : invalid) {
        assert(!patch(json, current, candidate).ok());
        assert(config::equalSettings(candidate, before));
    }
}

void testAllNumericBounds() {
    const auto schema = config::settingsSchema();
    for (size_t index = 0; index < schema.size; ++index) {
        const auto &field = schema.data[index];
        if (std::holds_alternative<bool DeviceSettings::*>(field.member)) continue;
        for (uint32_t value : {field.minimum, field.maximum}) {
            const std::string json =
                "{\"" + std::string(field.name) + "\":" + std::to_string(value) + "}";
            DeviceSettings candidate;
            assert(patch(json.c_str(), DeviceSettings{}, candidate).ok());
        }
        const std::string tooHigh = "{\"" + std::string(field.name) + "\":" +
                                   std::to_string(field.maximum + 1) + "}";
        DeviceSettings candidate;
        assert(!patch(tooHigh.c_str(), DeviceSettings{}, candidate).ok());
        if (field.minimum > 0) {
            const std::string tooLow = "{\"" + std::string(field.name) + "\":" +
                                      std::to_string(field.minimum - 1) + "}";
            assert(!patch(tooLow.c_str(), DeviceSettings{}, candidate).ok());
        }
    }
}

void testRecordVersionAndCapacity() {
    DynamicJsonDocument document(config::settingsJsonCapacity);
    const auto settings = fixture();
    assert(config::writeSettingsRecord(document.to<JsonObject>(), settings).ok());
    DeviceSettings decoded;
    assert(config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).ok());
    assert(config::equalSettings(decoded, settings));
    document["schemaVersion"] = 99;
    const auto before = decoded;
    assert(config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).code ==
           core::ErrorCode::UnsupportedVersion);
    assert(config::equalSettings(decoded, before));
    document["schemaVersion"] = 1;
    for (const auto *name : {"beeperPin", "fanEnabled", "fanPin", "fanFrequencyHz",
                             "udpListenPort", "blePresenceTimeoutSeconds"}) {
        document["settings"].as<JsonObject>().remove(name);
    }
    assert(config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).ok());
    assert(decoded.fanPin == 25 && decoded.beeperPin == 26 && !decoded.fanEnabled);
    assert(decoded.blePresenceTimeoutSeconds == 60 && decoded.udpListenPort == 0);
    document.clear();
    document["schemaVersion"] = 2;
    auto oldSettings = document.createNestedObject("settings");
    assert(config::writeSettingsJson(oldSettings, settings).ok());
    for (const auto *name : {"serial1Protocol", "serial2Protocol", "serial1DataBits",
                             "serial2DataBits", "serial1Parity", "serial2Parity",
                             "serial1StopBits", "serial2StopBits"}) {
        oldSettings.remove(name);
    }
    assert(config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).ok());
            assert(decoded.serial1Protocol == 0 && decoded.serial2DataBits == 8 &&
                decoded.serial2Route == 0 && !decoded.ebusWritesEnabled);
    document.clear();
    assert(config::writeSettingsRecord(document.to<JsonObject>(), settings).ok());
    document["schemaVersion"] = config::settingsRecordVersion;
    document["settings"].as<JsonObject>().remove("sensorIntervalMs");
    assert(!config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).ok());
    document["settings"]["sensorIntervalMs"] = 1000;
    document["extra"] = 1;
    assert(!config::readSettingsRecord(document.as<JsonVariantConst>(), decoded).ok());
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> small;
    assert(config::writeSettingsJson(small.to<JsonObject>(), settings).code ==
           core::ErrorCode::CapacityExceeded);
    assert(small.overflowed());
}

void testLegacyImportAndProfileDefaults() {
    Preferences::reset();
    config::PreferencesSettingsStore store;
    DeviceSettings loaded;
    const auto current = fixture();
    seedLegacy(current);
    Preferences::values["temp1_on"] = uint8_t{1};
    Preferences::values["temp2_on"] = uint8_t{0};
    Preferences::values["rf24_saved"] = String{"[]"};
    Preferences::values["ssid"] = String{"test-network"};
    assert(store.load(DeviceSettings{}, loaded).ok());
    assert(config::equalSettings(loaded, current));
    assert(Preferences::writes == 0 && !Preferences::values.count("device_cfg"));
    assert(std::get<String>(Preferences::values.at("ssid")) == "test-network");

    Preferences::reset();
    Preferences::values["uart1_mode"] = uint8_t{2};
    assert(store.load(config::defaultSettings(config::Uart1Mode::Gps), loaded).ok());
    assert(loaded.uart1Mode == 2 && loaded.uart1Baud == 115200 && !loaded.gpsEnabled);
    Preferences::reset();
    assert(store.load(config::defaultSettings(config::Uart1Mode::Gps), loaded).ok());
    assert(loaded.uart1Mode == 1 && loaded.uart1Baud == 9600 && loaded.gpsEnabled);
}

void testLegacyCorruption() {
    Preferences::reset();
    config::PreferencesSettingsStore store;
    DeviceSettings loaded = fixture();
    const auto before = loaded;
    Preferences::values["sensor_ms"] = uint32_t{1000};
    assert(!store.load(DeviceSettings{}, loaded).ok());
    assert(config::equalSettings(loaded, before));
    Preferences::reset();
    Preferences::values["display_bright"] = uint8_t{255};
    assert(store.load(DeviceSettings{}, loaded).ok() && loaded.displayBrightness == 255);
    Preferences::failRead = true;
    assert(!store.load(DeviceSettings{}, loaded).ok());
    Preferences::reset();
    Preferences::values["beep_on"] = uint8_t{2};
    assert(!store.load(DeviceSettings{}, loaded).ok());
    Preferences::reset();
    Preferences::values["sensor_ms"] = uint16_t{1};
    assert(!store.load(DeviceSettings{}, loaded).ok());
}

void testServicePersistenceAndRecovery() {
    Preferences::reset();
    config::PreferencesSettingsStore store;
    config::ConfigService service{store, DeviceSettings{}};
    assert(!service.ready());
    assert(!service.update(fixture()).ok());
    assert(service.begin().ok() && service.ready());
    assert(service.begin().ok() && Preferences::writes == 0);
    assert(service.update(service.settings()).ok() && Preferences::writes == 0);
    auto invalid = service.settings();
    invalid.displayMode = 255;
    assert(service.update(invalid).code == core::ErrorCode::InvalidArgument);
    assert(service.ready() && Preferences::writes == 0);
    assert(config::equalSettings(service.settings(), DeviceSettings{}));
    const auto settings = fixture();
    assert(service.update(settings).ok());
    assert(Preferences::writes == 1 && Preferences::values.count("device_cfg"));
    assert(config::equalSettings(service.settings(), settings));
    Preferences::values["sensor_ms"] = uint16_t{1000};
    config::ConfigService restarted{store, DeviceSettings{}};
    assert(restarted.begin().ok());
    assert(config::equalSettings(restarted.settings(), settings));

    DeviceSettings next = settings;
    next.sensorIntervalMs = 2000;
    Preferences::failWrite = true;
    assert(service.update(next).code == core::ErrorCode::StorageWriteFailed);
    assert(!service.ready() && config::equalSettings(service.settings(), settings));
    assert(service.update(settings).code == core::ErrorCode::StorageUnavailable);
    Preferences::failWrite = false;
    assert(service.begin().ok());
    assert(config::equalSettings(service.settings(), settings));

    Preferences::failReadAfterWrite = true;
    assert(service.update(next).code == core::ErrorCode::StorageWriteFailed);
    assert(!service.ready() && config::equalSettings(service.settings(), settings));
    Preferences::failRead = Preferences::failReadAfterWrite = false;
    assert(service.begin().ok());
    assert(config::equalSettings(service.settings(), next));
    Preferences::corruptAfterWrite = true;
    assert(service.update(settings).code == core::ErrorCode::StorageWriteFailed);
    assert(!service.ready());
    assert(config::equalSettings(service.settings(), next));
    assert(!service.begin().ok());
}

void testInvalidStoreAndVersionAreNotOverwritten() {
    Preferences::reset();
    Preferences::failOpen = true;
    config::PreferencesSettingsStore store;
    config::ConfigService service{store, DeviceSettings{}};
    assert(service.begin().code == core::ErrorCode::StorageUnavailable);
    assert(!service.ready());
    Preferences::failOpen = false;
    Preferences::values["device_cfg"] = String{"{\"schemaVersion\":99,\"settings\":{}}"};
    assert(service.begin().code == core::ErrorCode::UnsupportedVersion);
    assert(service.status().code == core::ErrorCode::UnsupportedVersion);
    assert(Preferences::writes == 0);
    assert(!service.update(fixture()).ok());
    assert(Preferences::writes == 0);
    Preferences::values["device_cfg"] = String{"broken"};
    assert(!service.begin().ok());
    Preferences::values["device_cfg"] = uint32_t{1};
    assert(!service.begin().ok());
    Preferences::values["device_cfg"] = String(config::settingsJsonCapacity + 1, ' ');
    assert(!service.begin().ok());
    assert(config::equalSettings(service.settings(), DeviceSettings{}));
    assert(Preferences::writes == 0);
}

}  // namespace

void runConfigTests() {
    testDefaultsAndSchema();
    testJsonCompatibilityAndRoundTrip();
    testPartialPatchAndTypes();
    testAllNumericBounds();
    testRecordVersionAndCapacity();
    testLegacyImportAndProfileDefaults();
    testLegacyCorruption();
    testServicePersistenceAndRecovery();
    testInvalidStoreAndVersionAreNotOverwritten();
    std::cout << "PASS: 9 configuration test groups (legacy import, schema, JSON, persistence)\n";
}
