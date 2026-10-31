#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {

class IBoardIoHardware {
public:
    virtual ~IBoardIoHardware() = default;
    virtual core::Result readInputs(uint8_t &bits) = 0;
    virtual core::Result readRelays(uint8_t &bits) = 0;
    virtual core::ActionResult writeRelay(uint8_t channel, bool on) = 0;
};

class BoardIo final : public core::IPeripheral, public core::IPropertyProvider,
                      public core::ITriggerProvider, public core::IActionProvider {
public:
    explicit BoardIo(IBoardIoHardware &hardware) : hardware_(hardware) {}
    const core::PeripheralDescriptor &descriptor() const override;
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::List<core::ActionDescriptor> actions() const override;
    core::Result begin(uint32_t nowMs) override;
    core::Result tick(uint32_t nowMs) override;
    void stop() override { started_ = false; health_ = {}; }
    core::Health health() const override { return health_; }
    core::Result readProperty(std::string_view id, core::PropertySnapshot &value) const override;
    core::ActionResult execute(std::string_view id, core::List<core::Parameter> parameters) override;

private:
    IBoardIoHardware &hardware_;
    std::array<core::PropertySnapshot, 14> snapshots_{};
    bool started_ = false;
    core::Health health_{};
};

}  // namespace kc868a6::peripherals
