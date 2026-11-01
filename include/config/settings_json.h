#pragma once

#include <ArduinoJson.h>

#include "config/device_settings.h"

namespace kc868a6::config {

constexpr uint32_t settingsRecordVersion = 3;
constexpr size_t settingsJsonCapacity = 4096;

core::Result readSettingsJson(JsonVariantConst source,
                             const DeviceSettings &current,
                             DeviceSettings &candidate, bool requireAll = false);
core::Result writeSettingsJson(JsonObject destination, const DeviceSettings &settings);
core::Result readSettingsRecord(JsonVariantConst source, DeviceSettings &settings);
core::Result writeSettingsRecord(JsonObject destination, const DeviceSettings &settings);
core::Result writeSettingsSchema(JsonObject destination);

}  // namespace kc868a6::config
