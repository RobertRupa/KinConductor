#include "peripherals/sensor_readings.h"
#include <cmath>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"sensors.main", "legacy-sensor-cache"};
const std::array<core::PropertyDescriptor, SensorReadings::count> descriptors{{
    {"sensors.main/temperature/1", core::ValueType::Number, "degC"},
    {"sensors.main/humidity/1", core::ValueType::Number, "%"},
    {"sensors.main/temperature/2", core::ValueType::Number, "degC"},
    {"sensors.main/humidity/2", core::ValueType::Number, "%"},
    {"sensors.main/air/temperature", core::ValueType::Number, "degC"},
    {"sensors.main/air/humidity", core::ValueType::Number, "%"},
    {"sensors.main/air/gas", core::ValueType::Number, "ohm"},
    {"sensors.main/air/aqi", core::ValueType::Number, ""},
    {"sensors.main/air/tvoc", core::ValueType::Number, "ppb"},
    {"sensors.main/air/eco2", core::ValueType::Number, "ppm"},
    {"sensors.main/gps/latitude", core::ValueType::Number, "deg"},
    {"sensors.main/gps/longitude", core::ValueType::Number, "deg"},
    {"sensors.main/gps/satellites", core::ValueType::Number, ""},
    {"sensors.main/compass/x", core::ValueType::Number, "raw"},
    {"sensors.main/compass/y", core::ValueType::Number, "raw"},
    {"sensors.main/compass/z", core::ValueType::Number, "raw"},
}};
const std::array<core::TriggerDescriptor, SensorReadings::count> conditions = [] {
    const char *ids[] = {
        "sensors.main/temperature/1/condition", "sensors.main/humidity/1/condition",
        "sensors.main/temperature/2/condition", "sensors.main/humidity/2/condition",
        "sensors.main/air/temperature/condition", "sensors.main/air/humidity/condition",
        "sensors.main/air/gas/condition", "sensors.main/air/aqi/condition",
        "sensors.main/air/tvoc/condition", "sensors.main/air/eco2/condition",
        "sensors.main/gps/latitude/condition", "sensors.main/gps/longitude/condition",
        "sensors.main/gps/satellites/condition", "sensors.main/compass/x/condition",
        "sensors.main/compass/y/condition", "sensors.main/compass/z/condition",
    };
    std::array<core::TriggerDescriptor, SensorReadings::count> result{};
    for (size_t index = 0; index < result.size(); ++index) {
        result[index] = {ids[index], core::TriggerKind::Condition, descriptors[index].id};
    }
    return result;
}();
}
const core::PeripheralDescriptor &SensorReadings::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> SensorReadings::properties() const { return descriptors; }
core::List<core::TriggerDescriptor> SensorReadings::triggers() const { return conditions; }
core::Result SensorReadings::readProperty(std::string_view id, core::PropertySnapshot &value) const {
    for (size_t index = 0; index < count; ++index) {
        if (descriptors[index].id == id) { value = values_[index]; return {}; }
    }
    return {core::ErrorCode::NotFound, id};
}
core::Result SensorReadings::publish(size_t index, double value, core::Quality quality, uint32_t sampledAt) {
    if (index >= count) return {core::ErrorCode::InvalidArgument, "sensor_index"};
    if (quality == core::Quality::Valid && !std::isfinite(value)) {
        values_[index].quality = core::Quality::Error;
        values_[index].error = core::ErrorCode::OperationFailed;
        return {core::ErrorCode::OperationFailed, descriptors[index].id};
    }
    values_[index] = {quality == core::Quality::Valid ? value : 0.0, quality, sampledAt};
    return {};
}
}
