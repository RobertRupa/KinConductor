#pragma once
#include <ArduinoJson.h>
#include "core/peripheral.h"

namespace kc868a6::peripherals {
inline constexpr size_t iftttRuleCapacity = 8;
inline constexpr size_t iftttActionCapacity = 8;
inline constexpr size_t udpRuleSocketCapacity = 4;
struct UdpRuleConfig {
    bool enabled = false;
    uint16_t port = 0;
    std::array<uint8_t, 4> remoteIp{};
    bool filterIp = false;
    uint16_t remotePort = 0;
    std::array<char, 257> text{};
    size_t textSize = 0;
    bool contains = false;
};
using UdpRulesConfig = std::array<UdpRuleConfig, iftttRuleCapacity>;
core::Result readUdpRule(JsonVariantConst source, UdpRuleConfig &destination);
core::Result readUdpRules(JsonVariantConst rules, uint16_t globalPort, UdpRulesConfig &destination);
bool matchesUdpRule(const UdpRuleConfig &config, const std::array<uint8_t, 4> &ip,
                    uint16_t port, std::string_view text);
class IUdpRulesHardware {
public:
    virtual ~IUdpRulesHardware() = default;
    virtual bool connected() const = 0;
    virtual core::Result open(uint8_t socket, uint16_t port) = 0;
    virtual void close(uint8_t socket) = 0;
    virtual core::Result receive(uint8_t socket, char *data, size_t capacity, size_t &size,
                                std::array<uint8_t, 4> &ip, uint16_t &port, bool &received) = 0;
};
class UdpRules final : public core::IPeripheral,
                       public core::IPropertyProvider,
                       public core::ITriggerProvider {
public:
    explicit UdpRules(IUdpRulesHardware &hardware) : hardware_(hardware) {}
    const core::PeripheralDescriptor &descriptor() const override;
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::Result begin(uint32_t now) override;
    void stop() override;
    core::Result tick(uint32_t now) override;
    core::Health health() const override { return health_; }
    core::Result readProperty(std::string_view id, core::PropertySnapshot &value) const override;
    void configure(const UdpRulesConfig &config);
    uint8_t consume(uint8_t rule);
private:
    IUdpRulesHardware &hardware_;
    UdpRulesConfig config_{};
    std::array<uint16_t, udpRuleSocketCapacity> ports_{};
    std::array<bool, udpRuleSocketCapacity> opened_{};
    std::array<uint8_t, iftttRuleCapacity> pending_{};
    std::array<uint32_t, iftttRuleCapacity> counts_{};
    std::array<core::PropertySnapshot, iftttRuleCapacity> snapshots_{};
    core::Health health_{};
    bool started_ = false;
};
}
