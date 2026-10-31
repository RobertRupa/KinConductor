#include "config/settings_json.h"

#include <limits>
#include <type_traits>

namespace kc868a6::config {
namespace {

const SettingDescriptor *findField(const char *name) {
    const auto schema = settingsSchema();
    for (size_t index = 0; index < schema.size; ++index) {
        if (std::string_view(schema.data[index].name) == name) return &schema.data[index];
    }
    return nullptr;
}

}  // namespace

core::Result readSettingsJson(JsonVariantConst source, const DeviceSettings &current,
                             DeviceSettings &candidate, bool requireAll) {
    if (!source.is<JsonObjectConst>()) {
        return {core::ErrorCode::InvalidArgument, "settings_object"};
    }
    const auto object = source.as<JsonObjectConst>();
    for (JsonPairConst pair : object) {
        if (findField(pair.key().c_str()) == nullptr) {
            return {core::ErrorCode::InvalidArgument, "unknown_setting"};
        }
    }
    DeviceSettings next = current;
    const auto schema = settingsSchema();
    for (size_t index = 0; index < schema.size; ++index) {
        const auto &field = schema.data[index];
        if (!object.containsKey(field.name)) {
            if (requireAll) return {core::ErrorCode::InvalidArgument, field.name};
            continue;
        }
        const auto value = object[field.name];
        const bool valid = std::visit(
            [&next, value](auto member) {
                using T = std::decay_t<decltype(next.*member)>;
                if constexpr (std::is_same_v<T, bool>) {
                    if (!value.is<bool>()) return false;
                } else {
                    if (!value.is<uint32_t>()) return false;
                    if (value.as<uint32_t>() > std::numeric_limits<T>::max()) return false;
                }
                next.*member = value.as<T>();
                return true;
            },
            field.member);
        if (!valid) return {core::ErrorCode::InvalidArgument, field.name};
    }
    normalizeSettings(next);
    const auto result = validateSettings(next);
    if (!result.ok()) return result;
    candidate = next;
    return {};
}

core::Result writeSettingsJson(JsonObject destination, const DeviceSettings &settings) {
    if (destination.isNull()) return {core::ErrorCode::CapacityExceeded, "settings_json"};
    const auto schema = settingsSchema();
    for (size_t index = 0; index < schema.size; ++index) {
        const auto &field = schema.data[index];
        const bool written = std::visit(
            [&settings, destination, &field](auto member) {
                return destination[field.name].set(settings.*member);
            },
            field.member);
        if (!written) return {core::ErrorCode::CapacityExceeded, "settings_json"};
    }
    return {};
}

core::Result readSettingsRecord(JsonVariantConst source, DeviceSettings &settings) {
    if (!source.is<JsonObjectConst>() || !source["schemaVersion"].is<uint32_t>()) {
        return {core::ErrorCode::InvalidArgument, "settings_record"};
    }
    const auto version = source["schemaVersion"].as<uint32_t>();
    if (version != 1 && version != settingsRecordVersion) {
        return {core::ErrorCode::UnsupportedVersion, "device_settings"};
    }
    const auto object = source.as<JsonObjectConst>();
    if (object.size() != 2 || !object.containsKey("settings")) {
        return {core::ErrorCode::InvalidArgument, "settings_record"};
    }
    if (version == 1) {
        const auto schema = settingsSchema();
        for (size_t index = 0; index < 27; ++index) {
            if (!object["settings"].containsKey(schema.data[index].name)) {
                return {core::ErrorCode::InvalidArgument, schema.data[index].name};
            }
        }
    }
    return readSettingsJson(object["settings"], DeviceSettings{}, settings,
                            version == settingsRecordVersion);
}

core::Result writeSettingsRecord(JsonObject destination, const DeviceSettings &settings) {
    if (!destination["schemaVersion"].set(settingsRecordVersion)) {
        return {core::ErrorCode::CapacityExceeded, "settings_record"};
    }

    return writeSettingsJson(destination.createNestedObject("settings"), settings);
}

core::Result writeSettingsSchema(JsonObject destination) {
    bool ok = destination["schemaVersion"].set(settingsRecordVersion);
    auto fields = destination.createNestedArray("fields");
    const auto schema = settingsSchema();
    for (size_t index = 0; index < schema.size && ok; ++index) {
        const auto &field = schema.data[index];
        auto item = fields.createNestedObject();
        const bool boolean = std::holds_alternative<bool DeviceSettings::*>(field.member);
        ok = item["id"].set(field.name) && item["type"].set(boolean ? "boolean" : "unsigned") &&
             item["minimum"].set(field.minimum) && item["maximum"].set(field.maximum) &&
             item["derived"].set(std::string_view(field.name) == "temperature1Enabled" ||
                                 std::string_view(field.name) == "temperature2Enabled");
        auto allowed = item.createNestedArray("allowedValues");
        for (size_t value = 0; value < field.allowedValues.size && ok; ++value) {
            ok = allowed.add(field.allowedValues.data[value]);
        }
        ok = ok && !allowed.isNull();
    }
    return ok && !fields.isNull() ? core::Result{}
        : core::Result{core::ErrorCode::CapacityExceeded, "settings_schema"};
}

}  // namespace kc868a6::config
