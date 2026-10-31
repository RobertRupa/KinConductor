#pragma once

#include <ArduinoJson.h>
#include "core/peripheral_registry.h"

namespace kc868a6::peripherals {
enum class CurrentSignal : uint8_t { Off = 0, Voltage = 1, CurrentLoop = 2 };
struct CurrentChannel {
    CurrentSignal signal = CurrentSignal::Off;
    bool calibrated = false;
    uint16_t rawLow = 0;
    uint16_t rawHigh = 4095;
    double signalLow = 4;
    double signalHigh = 20;
    double currentLow = 0;
    double currentHigh = 20;
};
struct CurrentConfig { std::array<CurrentChannel, 4> channels{}; };
constexpr size_t currentJsonCapacity = 4096;
core::Result validateCurrentConfig(const CurrentConfig &config);
core::Result readCurrentConfig(JsonVariantConst source, CurrentConfig &destination);
core::Result writeCurrentConfig(JsonObject destination, const CurrentConfig &config);
core::Result convertCurrent(const CurrentChannel &config, uint16_t raw,
                            double &signal, double &amperes);

class CurrentInputs final : public core::IPeripheral, public core::IPropertyProvider,
                            public core::ITriggerProvider {
public:
    explicit CurrentInputs(const core::PeripheralRegistry &registry);
    const core::PeripheralDescriptor &descriptor() const override;
    core::Result begin(uint32_t now) override;
    core::Result tick(uint32_t now) override;
    void stop() override;
    core::Health health() const override { return {started_, error_}; }
    core::List<core::PropertyDescriptor> properties() const override { return properties_; }
    core::List<core::TriggerDescriptor> triggers() const override;
    core::Result readProperty(std::string_view id, core::PropertySnapshot &value) const override;
    core::Result configure(const CurrentConfig &config);
    void unavailable(core::ErrorCode error);
    const CurrentConfig &config() const { return config_; }
private:
    const core::PeripheralRegistry &registry_;
    CurrentConfig config_;
    std::array<core::PropertyDescriptor, 8> properties_;
    std::array<core::PropertySnapshot, 8> snapshots_{};
    bool started_ = false;
    bool configReady_ = true;
    core::ErrorCode error_ = core::ErrorCode::None;
};
}
