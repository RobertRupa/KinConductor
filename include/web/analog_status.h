#pragma once

#include <ArduinoJson.h>

#include "peripherals/analog_inputs.h"

namespace kc868a6::web {

core::Result appendAnalogStatus(JsonArray destination,
                                const peripherals::AnalogInputs &inputs);

}  // namespace kc868a6::web
