#pragma once

#include <ArduinoJson.h>

#include "board/settings_resources.h"

namespace kc868a6::web {

constexpr size_t boardJsonCapacity =
    JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(3) +
    JSON_ARRAY_SIZE(board::pinCount) + board::pinCount * JSON_OBJECT_SIZE(6) +
    JSON_ARRAY_SIZE(board::ResourceManager::capacity) +
    board::ResourceManager::capacity * JSON_OBJECT_SIZE(3) + 256;
core::Result writeBoardStatus(JsonObject destination, const DeviceSettings &settings,
                             board::BoardOptions options, core::Result configStatus);

}  // namespace kc868a6::web
