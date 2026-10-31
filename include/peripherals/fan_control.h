#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {
class IFanHardware {
public:
    virtual ~IFanHardware() = default;
    virtual core::Result configure(bool enabled, uint8_t pin, uint16_t frequency) = 0;
    virtual core::Result write(uint8_t percent) = 0;
};

class FanControl final : public core::IPeripheral, public core::IPropertyProvider,
                         public core::ITriggerProvider, public core::IActionProvider {
public:
    explicit FanControl(IFanHardware &hardware) : hardware_(hardware) {}
    core::Result configure(bool enabled, uint8_t pin, uint16_t frequency, uint32_t now);
    const core::PeripheralDescriptor &descriptor() const override;
    core::Result begin(uint32_t now) override;
    core::Result tick(uint32_t now) override { now_ = now; return {}; }
    void stop() override;
    core::Health health() const override { return {started_, snapshot_.error}; }
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::List<core::ActionDescriptor> actions() const override;
    core::Result readProperty(std::string_view id, core::PropertySnapshot &value) const override;
    core::ActionResult execute(std::string_view id, core::List<core::Parameter> parameters) override;
private:
    IFanHardware &hardware_;
    bool started_ = false;
    bool enabled_ = false;
    uint8_t pin_ = 25;
    uint16_t frequency_ = 1000;
    uint32_t now_ = 0;
    core::PropertySnapshot snapshot_{uint32_t{0}, core::Quality::Disabled, 0, core::ErrorCode::None};
};
}
