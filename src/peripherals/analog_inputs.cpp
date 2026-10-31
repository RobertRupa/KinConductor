#include "peripherals/analog_inputs.h"

namespace kc868a6::peripherals {
namespace {

const core::PeripheralDescriptor analogDescriptor{"adc.main", "esp32-adc"};
const std::array<core::PropertyDescriptor, AnalogInputs::channelCount> analogProperties{{
    {"adc.main/channel/1/raw", core::ValueType::UnsignedInteger, "raw_adc"},
    {"adc.main/channel/2/raw", core::ValueType::UnsignedInteger, "raw_adc"},
    {"adc.main/channel/3/raw", core::ValueType::UnsignedInteger, "raw_adc"},
    {"adc.main/channel/4/raw", core::ValueType::UnsignedInteger, "raw_adc"},
}};
const std::array<core::TriggerDescriptor, AnalogInputs::channelCount> analogTriggers{{
    {"adc.main/channel/1/condition", core::TriggerKind::Condition, analogProperties[0].id},
    {"adc.main/channel/2/condition", core::TriggerKind::Condition, analogProperties[1].id},
    {"adc.main/channel/3/condition", core::TriggerKind::Condition, analogProperties[2].id},
    {"adc.main/channel/4/condition", core::TriggerKind::Condition, analogProperties[3].id},
}};

}  // namespace

AnalogInputs::AnalogInputs(IAnalogInputHardware &hardware,
                           std::array<uint8_t, channelCount> pins)
    : hardware_(hardware), pins_(pins) {}

const core::PeripheralDescriptor &AnalogInputs::descriptor() const {
    return analogDescriptor;
}

core::List<core::PropertyDescriptor> AnalogInputs::properties() const {
    return analogProperties;
}

core::List<core::TriggerDescriptor> AnalogInputs::triggers() const {
    return analogTriggers;
}

core::Result AnalogInputs::begin(uint32_t) {
    if (started_) return {};
    const core::Result result = hardware_.begin();
    health_ = {result.ok(), result.code};
    started_ = result.ok();
    values_.fill(0);
    snapshots_.fill({});
    return result;
}

core::Result AnalogInputs::tick(uint32_t nowMs) {
    if (!started_) return {core::ErrorCode::Unavailable, descriptor().id};
    core::Result firstError;
    for (size_t channel = 0; channel < channelCount; ++channel) {
        uint16_t value = 0;
        core::Result result = hardware_.read(pins_[channel], value);
        if (result.ok() && value > maxRawValue) {
            result = {core::ErrorCode::InvalidArgument, analogProperties[channel].id};
        }
        if (result.ok()) {
            values_[channel] = value;
            snapshots_[channel] = {
                static_cast<uint32_t>(value), core::Quality::Valid, nowMs,
                core::ErrorCode::None};
        } else {
            snapshots_[channel].quality = core::Quality::Error;
            snapshots_[channel].error = result.code;
            if (firstError.ok()) firstError = result;
        }
    }
    health_ = {firstError.ok(), firstError.code};
    return firstError;
}

void AnalogInputs::stop() {
    started_ = false;
    health_ = {};
    snapshots_.fill({});
}

core::Result AnalogInputs::readProperty(std::string_view id,
                                      core::PropertySnapshot &snapshot) const {
    snapshot = {};
    for (size_t channel = 0; channel < channelCount; ++channel) {
        if (analogProperties[channel].id == id) {
            snapshot = snapshots_[channel];
            return {};
        }
    }
    return {core::ErrorCode::NotFound, id};
}

}  // namespace kc868a6::peripherals
