#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {
class SensorReadings final : public core::IPeripheral, public core::IPropertyProvider,
                             public core::ITriggerProvider {
public:
    static constexpr size_t count = 16;
    const core::PeripheralDescriptor &descriptor() const override;
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::Result begin(uint32_t) override { started_ = true; return {}; }
    core::Result tick(uint32_t) override { return {}; }
    void stop() override { started_ = false; values_.fill({}); }
    core::Health health() const override { return {started_, core::ErrorCode::None}; }
    core::Result readProperty(std::string_view id, core::PropertySnapshot &value) const override;
    core::Result publish(size_t index, double value, core::Quality quality, uint32_t sampledAt);
private:
    bool started_ = false;
    std::array<core::PropertySnapshot, count> values_{};
};
}
