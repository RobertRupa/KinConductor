#include "peripherals/current_inputs.h"
#include <cmath>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"current.main", "calibrated-current"};
const char *const rawIds[] = {"adc.main/channel/1/raw", "adc.main/channel/2/raw",
                             "adc.main/channel/3/raw", "adc.main/channel/4/raw"};
const std::array<core::PropertyDescriptor, 8> descriptors{{
    {"current.main/channel/1/amperes", core::ValueType::Number, "A"},
    {"current.main/channel/1/signal", core::ValueType::Number, "mA"},
    {"current.main/channel/2/amperes", core::ValueType::Number, "A"},
    {"current.main/channel/2/signal", core::ValueType::Number, "mA"},
    {"current.main/channel/3/amperes", core::ValueType::Number, "A"},
    {"current.main/channel/3/signal", core::ValueType::Number, "mA"},
    {"current.main/channel/4/amperes", core::ValueType::Number, "A"},
    {"current.main/channel/4/signal", core::ValueType::Number, "mA"},
}};
const std::array<core::TriggerDescriptor, 8> conditions{{
    {"current.main/channel/1/amperes/condition", core::TriggerKind::Condition, descriptors[0].id},
    {"current.main/channel/1/signal/condition", core::TriggerKind::Condition, descriptors[1].id},
    {"current.main/channel/2/amperes/condition", core::TriggerKind::Condition, descriptors[2].id},
    {"current.main/channel/2/signal/condition", core::TriggerKind::Condition, descriptors[3].id},
    {"current.main/channel/3/amperes/condition", core::TriggerKind::Condition, descriptors[4].id},
    {"current.main/channel/3/signal/condition", core::TriggerKind::Condition, descriptors[5].id},
    {"current.main/channel/4/amperes/condition", core::TriggerKind::Condition, descriptors[6].id},
    {"current.main/channel/4/signal/condition", core::TriggerKind::Condition, descriptors[7].id},
}};
core::Result validateChannel(const CurrentChannel &channel) {
    const bool voltage = channel.signal == CurrentSignal::Voltage;
    if (channel.signal != CurrentSignal::Off && !voltage &&
        channel.signal != CurrentSignal::CurrentLoop) {
        return {core::ErrorCode::InvalidArgument, "current_signal"};
    }
    if (channel.rawLow >= channel.rawHigh || channel.rawHigh > 4095 ||
        !std::isfinite(channel.signalLow) || !std::isfinite(channel.signalHigh) ||
        !std::isfinite(channel.currentLow) || !std::isfinite(channel.currentHigh) ||
        channel.signalLow < 0 || channel.signalLow >= channel.signalHigh ||
        channel.signalHigh > (voltage ? 5.0 : 24.0) ||
        channel.currentLow < -100000 || channel.currentHigh > 100000 ||
        channel.currentLow >= channel.currentHigh) {
        return {core::ErrorCode::InvalidArgument, "current_calibration"};
    }
    return {};
}
}
core::Result validateCurrentConfig(const CurrentConfig &config) {
    for (const auto &channel : config.channels) {
        const auto result = validateChannel(channel);
        if (!result.ok()) return result;
    }
    return {};
}
core::Result readCurrentConfig(JsonVariantConst source, CurrentConfig &destination) {
    const auto invalid = core::Result{core::ErrorCode::InvalidArgument, "current_config"};
    if (!source.is<JsonObjectConst>() || source.size() != 2 ||
        !source["schemaVersion"].is<uint32_t>() || source["schemaVersion"].as<uint32_t>() != 1 ||
        !source["channels"].is<JsonArrayConst>() || source["channels"].size() != 4) return invalid;
    CurrentConfig next;
    size_t index = 0;
    for (JsonVariantConst value : source["channels"].as<JsonArrayConst>()) {
        if (!value.is<JsonObjectConst>() || value.size() != 8 ||
            !value["signal"].is<uint8_t>() || !value["calibrated"].is<bool>() ||
            !value["rawLow"].is<uint16_t>() || !value["rawHigh"].is<uint16_t>()) return invalid;
        for (const char *field : {"signalLow", "signalHigh", "currentLow", "currentHigh"}) {
            if (!value[field].is<double>() || value[field].is<bool>()) return invalid;
        }
        auto &channel = next.channels[index++];
        channel.signal = static_cast<CurrentSignal>(value["signal"].as<uint8_t>());
        channel.calibrated = value["calibrated"].as<bool>();
        channel.rawLow = value["rawLow"].as<uint16_t>();
        channel.rawHigh = value["rawHigh"].as<uint16_t>();
        channel.signalLow = value["signalLow"].as<double>();
        channel.signalHigh = value["signalHigh"].as<double>();
        channel.currentLow = value["currentLow"].as<double>();
        channel.currentHigh = value["currentHigh"].as<double>();
    }
    const auto valid = validateCurrentConfig(next);
    if (valid.ok()) destination = next;
    return valid;
}
core::Result writeCurrentConfig(JsonObject destination, const CurrentConfig &config) {
    bool ok = destination["schemaVersion"].set(1);
    auto channels = destination.createNestedArray("channels");
    for (const auto &channel : config.channels) {
        auto value = channels.createNestedObject();
        ok = ok && value["signal"].set(static_cast<uint8_t>(channel.signal)) &&
            value["calibrated"].set(channel.calibrated) &&
            value["rawLow"].set(channel.rawLow) && value["rawHigh"].set(channel.rawHigh) &&
            value["signalLow"].set(channel.signalLow) && value["signalHigh"].set(channel.signalHigh) &&
            value["currentLow"].set(channel.currentLow) && value["currentHigh"].set(channel.currentHigh);
    }
    return ok && !channels.isNull() ? core::Result{} :
        core::Result{core::ErrorCode::CapacityExceeded, "current_json"};
}
core::Result convertCurrent(const CurrentChannel &config, uint16_t raw,
                            double &signal, double &amperes) {
    const auto valid = validateChannel(config);
    if (!valid.ok()) return valid;
    if (config.signal == CurrentSignal::Off || !config.calibrated) {
        return {core::ErrorCode::Unavailable, "current_uncalibrated"};
    }
    if (raw < config.rawLow || raw > config.rawHigh) {
        return {core::ErrorCode::OperationFailed, "current_out_of_range"};
    }
    const double ratio = static_cast<double>(raw - config.rawLow) / (config.rawHigh - config.rawLow);
    signal = config.signalLow + ratio * (config.signalHigh - config.signalLow);
    amperes = config.currentLow + ratio * (config.currentHigh - config.currentLow);
    return {};
}
CurrentInputs::CurrentInputs(const core::PeripheralRegistry &registry)
    : registry_(registry), properties_(descriptors) {}
const core::PeripheralDescriptor &CurrentInputs::descriptor() const { return identity; }
core::List<core::TriggerDescriptor> CurrentInputs::triggers() const { return conditions; }
core::Result CurrentInputs::begin(uint32_t now) { started_ = true; return tick(now); }
void CurrentInputs::stop() { started_ = false; snapshots_.fill({}); }
core::Result CurrentInputs::configure(const CurrentConfig &config) {
    const auto valid = validateCurrentConfig(config);
    if (!valid.ok()) return valid;
    config_ = config;
    configReady_ = true;
    error_ = core::ErrorCode::None;
    for (size_t channel = 0; channel < 4; ++channel) {
        properties_[channel * 2 + 1].unit = config.channels[channel].signal == CurrentSignal::Voltage ? "V" : "mA";
    }
    snapshots_.fill({});
    return {};
}
void CurrentInputs::unavailable(core::ErrorCode error) {
    configReady_ = false;
    error_ = error;
    for (auto &snapshot : snapshots_) snapshot = {0.0, core::Quality::Error, 0, error};
}
core::Result CurrentInputs::tick(uint32_t now) {
    if (!started_) return {core::ErrorCode::Unavailable, "current_module"};
    if (!configReady_) return {error_, "current_config"};
    core::Result firstError;
    for (size_t channel = 0; channel < 4; ++channel) {
        const auto &config = config_.channels[channel];
        core::PropertySnapshot amperes{0.0, core::Quality::Disabled, now};
        core::PropertySnapshot signal = amperes;
        if (config.signal != CurrentSignal::Off) {
            amperes.quality = signal.quality = core::Quality::WarmingUp;
            if (config.calibrated) {
                core::PropertySnapshot raw;
                auto result = registry_.readProperty(rawIds[channel], now, raw);
                double input = 0, value = 0;
                if (result.ok() && raw.quality == core::Quality::Valid) {
                    const auto *number = std::get_if<uint32_t>(&raw.value);
                    result = number && *number <= 4095 ? convertCurrent(config, *number, input, value) :
                        core::Result{core::ErrorCode::InvalidArgument, "current_raw"};
                    amperes = {value, result.ok() ? core::Quality::Valid : core::Quality::Error,
                               raw.updatedAtMs, result.code};
                    signal = {input, amperes.quality, raw.updatedAtMs, result.code};
                } else {
                    amperes = {0.0, result.ok() ? raw.quality : core::Quality::Error,
                               raw.updatedAtMs, result.ok() ? raw.error : result.code};
                    signal = amperes;
                }
                if (!result.ok() && firstError.ok()) firstError = result;
            }
        }
        snapshots_[channel * 2] = amperes;
        snapshots_[channel * 2 + 1] = signal;
    }
    error_ = firstError.code;
    return firstError;
}
core::Result CurrentInputs::readProperty(std::string_view id, core::PropertySnapshot &value) const {
    for (size_t index = 0; index < properties_.size(); ++index) {
        if (properties_[index].id == id) { value = snapshots_[index]; return {}; }
    }
    return {core::ErrorCode::NotFound, "current_property"};
}
}
