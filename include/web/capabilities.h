#pragma once

#include <ArduinoJson.h>
#include "core/peripheral_registry.h"

namespace kc868a6::web {
constexpr size_t capabilitiesJsonCapacity = JSON_OBJECT_SIZE(800) + 8192;
const char *valueTypeName(core::ValueType type);
const char *qualityName(core::Quality quality);
core::Result writeCapabilities(JsonObject destination, const core::PeripheralRegistry &registry,
                               uint32_t nowMs);
core::ActionResult executeJsonAction(core::PeripheralRegistry &registry, JsonVariantConst request,
                                    bool execute = true);
}
