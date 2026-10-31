#pragma once
#include <ArduinoJson.h>
#include "peripherals/ble_presence.h"
namespace kc868a6::web {
constexpr size_t bleScanJsonCapacity = JSON_OBJECT_SIZE(1) +
    JSON_ARRAY_SIZE(peripherals::BleScanSnapshot::capacity) +
    peripherals::BleScanSnapshot::capacity * (JSON_OBJECT_SIZE(4) + 64);
core::Result writeBleScan(JsonObject destination, const peripherals::BleScanSnapshot &scan);
}
