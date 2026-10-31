#include "config/preferences_settings_store.h"
#include "config/settings_json.h"

#include <limits>
#include <type_traits>

namespace kc868a6::config {
namespace {

constexpr const char *recordKey = "device_cfg";

template <typename T>
bool readLegacyField(Preferences &preferences, const char *key, T &value) {
    if (!preferences.isKey(key)) return true;
    if constexpr (std::is_same_v<T, bool>) {
        if (preferences.getType(key) != PT_U8) return false;
        const uint8_t stored = preferences.getUChar(key, UINT8_MAX);
        if (stored > 1) return false;
        value = stored != 0;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        if (preferences.getType(key) != PT_U8) return false;
        value = preferences.getUChar(key, UINT8_MAX);
        // 255 is a valid brightness; verify the read rather than using it as a sentinel.
        if (value == UINT8_MAX) {
            const uint8_t second = preferences.getUChar(key, 0);
            if (second != value) return false;
        }
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        if (preferences.getType(key) != PT_U16) return false;
        value = preferences.getUShort(key, UINT16_MAX);
        if (value == UINT16_MAX) return false;
    } else {
        if (preferences.getType(key) != PT_U32) return false;
        value = preferences.getUInt(key, UINT32_MAX);
        if (value == UINT32_MAX) return false;
    }
    return true;
}

}  // namespace

core::Result PreferencesSettingsStore::open() {
    if (opened_) return {};
    if (!preferences_.begin("kc868a6", false)) {
        return {core::ErrorCode::StorageUnavailable, "kc868a6"};
    }
    opened_ = true;
    return {};
}

core::Result PreferencesSettingsStore::load(const DeviceSettings &defaults,
                                          DeviceSettings &settings) {
    const auto opened = open();
    if (!opened.ok()) return opened;
    if (preferences_.isKey(recordKey)) {
        if (preferences_.getType(recordKey) != PT_STR) {
            return {core::ErrorCode::InvalidArgument, "device_cfg"};
        }
        const size_t length = preferences_.getStringLength(recordKey);
        if (length == 0 || length > settingsJsonCapacity) {
            return {core::ErrorCode::InvalidArgument, "device_cfg"};
        }
        DynamicJsonDocument document(settingsJsonCapacity);
        const String stored = preferences_.getString(recordKey);
        if (deserializeJson(document, stored)) {
            return {core::ErrorCode::InvalidArgument, "device_cfg"};
        }
        return readSettingsRecord(document.as<JsonVariantConst>(), settings);
    }

    DeviceSettings candidate = defaults;
    const auto schema = settingsSchema();
    // UART mode determines defaults for legacy keys that may not exist yet.
    if (!readLegacyField(preferences_, "uart1_mode", candidate.uart1Mode)) {
        return {core::ErrorCode::InvalidArgument, "uart1_mode"};
    }
    candidate.uart1Baud = candidate.uart1Mode == 2 ? 115200 : 9600;
    candidate.gpsEnabled = candidate.uart1Mode == 1;
    for (size_t index = 0; index < schema.size; ++index) {
        const auto &field = schema.data[index];
        if (std::string_view(field.name) == "temperature1Enabled" ||
            std::string_view(field.name) == "temperature2Enabled") {
            continue;
        }
        const bool read = std::visit(
            [&candidate, &field, this](auto member) {
                return readLegacyField(preferences_, field.legacyKey, candidate.*member);
            },
            field.member);
        if (!read) return {core::ErrorCode::InvalidArgument, field.name};
    }
    normalizeSettings(candidate);
    const auto valid = validateSettings(candidate);
    if (!valid.ok()) return valid;
    settings = candidate;
    return {};
}

core::Result PreferencesSettingsStore::save(const DeviceSettings &settings) {
    const auto opened = open();
    if (!opened.ok()) return opened;
    const auto valid = validateSettings(settings);
    if (!valid.ok()) return valid;
    DynamicJsonDocument document(settingsJsonCapacity);
    const auto written = writeSettingsRecord(document.to<JsonObject>(), settings);
    if (!written.ok()) return written;
    String encoded;
    if (serializeJson(document, encoded) == 0) {
        return {core::ErrorCode::CapacityExceeded, "device_cfg"};
    }
    // One NVS value replaces the previous multi-key write, so no mixed settings are committed.
    if (preferences_.putString(recordKey, encoded) != encoded.length() ||
        preferences_.getString(recordKey) != encoded) {
        return {core::ErrorCode::StorageWriteFailed, "device_cfg"};
    }
    return {};
}

}  // namespace kc868a6::config
