#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {

class IAnalogInputHardware {
public:
    virtual ~IAnalogInputHardware() = default;
    virtual core::Result begin() = 0;
    virtual core::Result read(uint8_t pin, uint16_t &value) = 0;
};

class AnalogInputs final : public core::IPeripheral,
                           public core::IPropertyProvider,
                           public core::ITriggerProvider {
public:
    static constexpr size_t channelCount = 4;
    static constexpr uint16_t maxRawValue = 4095;

    AnalogInputs(IAnalogInputHardware &hardware,
                 std::array<uint8_t, channelCount> pins);

    const core::PeripheralDescriptor &descriptor() const override;
    core::Result begin(uint32_t nowMs) override;
    core::Result tick(uint32_t nowMs) override;
    void stop() override;
    core::Health health() const override { return health_; }
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::Result readProperty(std::string_view id,
                              core::PropertySnapshot &snapshot) const override;

    const std::array<uint16_t, channelCount> &rawValues() const { return values_; }

private:
    IAnalogInputHardware &hardware_;
    std::array<uint8_t, channelCount> pins_;
    std::array<uint16_t, channelCount> values_{};
    std::array<core::PropertySnapshot, channelCount> snapshots_{};
    core::Health health_;
    bool started_ = false;
};

}  // namespace kc868a6::peripherals
