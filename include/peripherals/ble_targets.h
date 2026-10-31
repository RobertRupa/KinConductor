#pragma once

#include <ArduinoJson.h>
#include "peripherals/ble_presence.h"

namespace kc868a6::peripherals {
constexpr size_t bleTargetsJsonCapacity = JSON_OBJECT_SIZE(3) +
    JSON_ARRAY_SIZE(bleTargetCapacity) + 256;
core::Result readBleTargets(JsonVariantConst source, BleTargets &targets);
core::Result writeBleTargets(JsonObject destination, const BleTargets &targets);
}
