#include "peripherals/fan_control.h"

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"fan.main", "pwm_control"};
const std::array<core::PropertyDescriptor, 1> props{{
    {"fan.main/duty", core::ValueType::UnsignedInteger, "%", 0}
}};
const std::array<core::TriggerDescriptor, 1> conditions{{
    {"fan.main/duty/condition", core::TriggerKind::Condition, "fan.main/duty"}
}};
const std::array<core::ParameterDescriptor, 1> params{{
    {"percent", core::ValueType::UnsignedInteger, true, 0.0, 100.0}
}};
const std::array<core::ActionDescriptor, 1> commands{{{"fan.main/set", params}}};
}
const core::PeripheralDescriptor &FanControl::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> FanControl::properties() const { return props; }
core::List<core::TriggerDescriptor> FanControl::triggers() const { return conditions; }
core::List<core::ActionDescriptor> FanControl::actions() const { return commands; }
core::Result FanControl::begin(uint32_t now) {
    started_ = true;
    return configure(enabled_, pin_, frequency_, now);
}
core::Result FanControl::configure(bool enabled, uint8_t pin, uint16_t frequency, uint32_t now) {
    if ((pin != 25 && pin != 26) || frequency < 100 || frequency > 25000) {
        return {core::ErrorCode::InvalidArgument, "fan_config"};
    }
    now_ = now;
    enabled_ = enabled;
    pin_ = pin;
    frequency_ = frequency;
    const auto result = hardware_.configure(enabled, pin, frequency);
    snapshot_ = {uint32_t{0}, !result.ok() ? core::Quality::Error :
        enabled ? core::Quality::Valid : core::Quality::Disabled, now, result.code};
    return result;
}
void FanControl::stop() {
    const auto result = hardware_.configure(false, pin_, frequency_);
    snapshot_ = {uint32_t{0}, result.ok() ? core::Quality::Disabled : core::Quality::Error,
                 now_, result.code};
    started_ = false;
    enabled_ = false;
}
core::Result FanControl::readProperty(std::string_view id, core::PropertySnapshot &value) const {
    if (id != props[0].id) return {core::ErrorCode::NotFound, "fan_property"};
    value = snapshot_;
    return {};
}
core::ActionResult FanControl::execute(std::string_view id, core::List<core::Parameter> parameters) {
    if (id != commands[0].id) return {{core::ErrorCode::NotFound, "fan_action"}, false, false};
    if (!started_ || !enabled_ || snapshot_.quality == core::Quality::Error) {
        return {{core::ErrorCode::Unavailable, "fan_disabled"}, false, false};
    }
    if (!parameters.data || parameters.size != 1 || parameters.data[0].id != "percent" ||
        !core::valueMatchesParameter(parameters.data[0].value, params[0])) {
        return {{core::ErrorCode::InvalidArgument, "percent"}, false, false};
    }
    const auto percent = std::get<uint32_t>(parameters.data[0].value);
    const auto result = hardware_.write(static_cast<uint8_t>(percent));
    if (!result.ok()) {
        snapshot_.quality = core::Quality::Error;
        snapshot_.error = result.code;
        return {result, false, false};
    }
    snapshot_ = {percent, core::Quality::Valid, now_, core::ErrorCode::None};
    return {{}, true, false};
}
}
