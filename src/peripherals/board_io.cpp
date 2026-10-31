#include "peripherals/board_io.h"

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"io.main", "pcf8574-io"};
const std::array<core::PropertyDescriptor, 14> properties{{
    {"io.main/input/1", core::ValueType::Boolean, ""},
    {"io.main/input/2", core::ValueType::Boolean, ""},
    {"io.main/input/3", core::ValueType::Boolean, ""},
    {"io.main/input/4", core::ValueType::Boolean, ""},
    {"io.main/input/5", core::ValueType::Boolean, ""},
    {"io.main/input/6", core::ValueType::Boolean, ""},
    {"io.main/input/7", core::ValueType::Boolean, ""},
    {"io.main/input/8", core::ValueType::Boolean, ""},
    {"io.main/relay/1", core::ValueType::Boolean, ""},
    {"io.main/relay/2", core::ValueType::Boolean, ""},
    {"io.main/relay/3", core::ValueType::Boolean, ""},
    {"io.main/relay/4", core::ValueType::Boolean, ""},
    {"io.main/relay/5", core::ValueType::Boolean, ""},
    {"io.main/relay/6", core::ValueType::Boolean, ""},
}};
const std::array<core::TriggerDescriptor, 14> triggers = [] {
    std::array<core::TriggerDescriptor, 14> result{};
    const char *ids[] = {
        "io.main/input/1/condition", "io.main/input/2/condition",
        "io.main/input/3/condition", "io.main/input/4/condition",
        "io.main/input/5/condition", "io.main/input/6/condition",
        "io.main/input/7/condition", "io.main/input/8/condition",
        "io.main/relay/1/condition", "io.main/relay/2/condition",
        "io.main/relay/3/condition", "io.main/relay/4/condition",
        "io.main/relay/5/condition", "io.main/relay/6/condition",
    };
    for (size_t index = 0; index < result.size(); ++index) {
        result[index] = {ids[index], core::TriggerKind::Condition, properties[index].id};
    }
    return result;
}();
const std::array<core::ParameterDescriptor, 2> parameters{{
    {"channel", core::ValueType::UnsignedInteger, true, 1.0, 6.0},
    {"on", core::ValueType::Boolean, true},
}};
const std::array<core::ActionDescriptor, 1> actions{{
    {"io.main/relay/set", parameters},
}};
}

const core::PeripheralDescriptor &BoardIo::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> BoardIo::properties() const { return peripherals::properties; }
core::List<core::TriggerDescriptor> BoardIo::triggers() const { return peripherals::triggers; }
core::List<core::ActionDescriptor> BoardIo::actions() const { return peripherals::actions; }

core::Result BoardIo::begin(uint32_t nowMs) {
    (void)nowMs;
    started_ = true;
    health_ = {true, core::ErrorCode::None};
    return {};
}

core::Result BoardIo::tick(uint32_t nowMs) {
    if (!started_) return {core::ErrorCode::Unavailable, identity.id};
    core::Result first;
    for (size_t group = 0; group < 2; ++group) {
        uint8_t bits = 0xFF;
        const auto result = group == 0 ? hardware_.readInputs(bits) : hardware_.readRelays(bits);
        const size_t offset = group == 0 ? 0 : 8;
        const size_t count = group == 0 ? 8 : 6;
        for (size_t channel = 0; channel < count; ++channel) {
            auto &snapshot = snapshots_[offset + channel];
            if (result.ok()) {
                snapshot = {(bits & (1U << channel)) == 0, core::Quality::Valid, nowMs};
            } else {
                snapshot.quality = core::Quality::Error;
                snapshot.error = result.code;
            }
        }
        if (!result.ok() && first.ok()) first = result;
    }
    // A transient input read failure must not prevent independent relay actions/recovery.
    health_ = {true, first.code};
    return first;
}

core::Result BoardIo::readProperty(std::string_view id, core::PropertySnapshot &value) const {
    for (size_t index = 0; index < peripherals::properties.size(); ++index) {
        if (peripherals::properties[index].id == id) {
            value = snapshots_[index];
            return {};
        }
    }
    return {core::ErrorCode::NotFound, id};
}

core::ActionResult BoardIo::execute(std::string_view id, core::List<core::Parameter> args) {
    if (id != actions().data[0].id) return {{core::ErrorCode::NotFound, id}};
    const uint32_t *channel = nullptr;
    const bool *on = nullptr;
    if (args.size != 2 || !args.data) return {{core::ErrorCode::InvalidArgument, id}};
    for (size_t index = 0; index < args.size; ++index) {
        if (args.data[index].id == "channel") channel = std::get_if<uint32_t>(&args.data[index].value);
        else if (args.data[index].id == "on") on = std::get_if<bool>(&args.data[index].value);
        else return {{core::ErrorCode::InvalidArgument, args.data[index].id}};
    }
    if (!channel || !on || *channel < 1 || *channel > 6) {
        return {{core::ErrorCode::InvalidArgument, "channel"}};
    }
    return hardware_.writeRelay(static_cast<uint8_t>(*channel - 1), *on);
}
}  // namespace kc868a6::peripherals
