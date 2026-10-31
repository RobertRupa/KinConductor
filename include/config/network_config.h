#pragma once

#include <ArduinoJson.h>
#include "core/peripheral.h"

namespace kc868a6::config {
struct NetworkConfig { std::array<char, 64> hostname{{'k','i','n','c','o','n','d','u','c','t','o','r','\0'}}; };
core::Result normalizeHostname(std::string_view source, NetworkConfig &destination);
core::Result readNetworkConfig(JsonVariantConst source, NetworkConfig &destination);
core::Result writeNetworkConfig(JsonObject destination, const NetworkConfig &config);
}
