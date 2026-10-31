#include "web/analog_status.h"

namespace kc868a6::web {

core::Result appendAnalogStatus(JsonArray destination,
                                const peripherals::AnalogInputs &inputs) {
    for (uint16_t value : inputs.rawValues()) {
        if (!destination.add(value)) {
            return {core::ErrorCode::CapacityExceeded, "analog_status"};
        }
    }
    return {};
}

}  // namespace kc868a6::web
