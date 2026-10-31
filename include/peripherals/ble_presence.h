#pragma once

#include "core/peripheral.h"

namespace kc868a6::peripherals {

using BleAddress = std::array<uint8_t, 6>;
constexpr size_t bleTargetCapacity = 8;
struct BleTargets {
    std::array<BleAddress, bleTargetCapacity> addresses{};
    size_t count = 0;
    BleAddress legacyAddress{};
};

struct BleScanDevice {
    BleAddress address{};
    std::array<char, 32> name{};
    int16_t rssi = 0;
    uint8_t addressType = 0;
    uint32_t seenAtMs = 0;
};

struct BleScanSnapshot {
    static constexpr size_t capacity = 24;
    std::array<BleScanDevice, capacity> devices{};
    size_t count = 0;
    std::array<bool, bleTargetCapacity> targetObserved{};
    std::array<uint32_t, bleTargetCapacity> targetSeenAtMs{};
};

// Implementations synchronize callbacks internally; poll consumes sightings.
// Saved targets are tracked independently of the bounded UI cache.
class IBlePresenceScanner {
public:
    virtual ~IBlePresenceScanner() = default;
    virtual core::Result start() = 0;
    virtual void stop() = 0;
    virtual void setTargets(const BleTargets &targets) = 0;
    virtual core::Result poll(BleScanSnapshot &snapshot) = 0;
};

class BlePresence final : public core::IPeripheral,
                          public core::IPropertyProvider,
                          public core::ITriggerProvider {
public:
    static constexpr uint32_t defaultTimeoutMs = 15000;
    static constexpr uint32_t minimumTimeoutMs = 1000;
    static constexpr uint32_t maximumTimeoutMs = 86400000;
    static constexpr uint32_t scanResultLifetimeMs = 30000;
    static constexpr std::string_view propertyId = "ble.presence/in_range";

    explicit BlePresence(IBlePresenceScanner &scanner);
    BlePresence(const BlePresence &) = delete;
    BlePresence &operator=(const BlePresence &) = delete;
    const core::PeripheralDescriptor &descriptor() const override;
    core::List<core::PropertyDescriptor> properties() const override;
    core::List<core::TriggerDescriptor> triggers() const override;
    core::Result begin(uint32_t nowMs) override;
    core::Result tick(uint32_t nowMs) override;
    void stop() override;
    core::Health health() const override { return health_; }
    core::Result readProperty(std::string_view id,
                              core::PropertySnapshot &snapshot) const override;

    // Empty clears the target. Otherwise exactly XX:XX:XX:XX:XX:XX is required.
    core::Result setTarget(std::string_view mac, uint32_t nowMs);
    core::Result setTargets(const BleTargets &targets, uint32_t nowMs);
    const BleTargets &targets() const { return targets_; }
    core::Result setTimeoutMs(uint32_t timeoutMs);
    // Call false BEFORE deinitializing/reconfiguring NimBLE. Only scan mode 1
    // with a successfully initialized stack may set this true.
    void setScanAvailable(bool available, uint32_t nowMs);
    std::string_view selectedMac() const { return legacyMac_.data(); }
    uint32_t timeoutMs() const { return timeoutMs_; }
    // Cached, immediate UI results; never starts a second synchronous scan.
    core::Result scanResults(uint32_t nowMs, BleScanSnapshot &snapshot) const;
    // Side-effect-free endpoint validation. Empty succeeds and clears output;
    // errors leave output unchanged. Successful nonempty output is uppercase.
    static core::Result normalizeTargetMac(std::string_view text,
                                          std::array<char, 18> &normalized);
    static core::Result validateTimeoutMs(uint32_t timeoutMs);
    static core::Result parseMac(std::string_view text, BleAddress &address);
    static std::array<char, 18> formatMac(const BleAddress &address);

private:
    void resetPresence(uint32_t nowMs);
    void setValue(size_t index, bool inRange, core::Quality quality, uint32_t nowMs,
                  core::ErrorCode error = core::ErrorCode::None);
    IBlePresenceScanner &scanner_;
    BleTargets targets_{};
    std::array<char, 18> legacyMac_{};
    std::array<std::array<char, 18>, bleTargetCapacity> macs_{};
    std::array<std::array<char, 40>, bleTargetCapacity> propertyIds_{};
    std::array<std::array<char, 40>, bleTargetCapacity> triggerIds_{};
    std::array<core::PropertyDescriptor, bleTargetCapacity + 1> properties_{};
    std::array<core::TriggerDescriptor, bleTargetCapacity + 2> triggers_{};
    BleScanSnapshot scan_{};
    std::array<core::PropertySnapshot, bleTargetCapacity> values_{};
    core::PropertySnapshot disabled_{false, core::Quality::Disabled};
    core::Health health_{};
    uint32_t timeoutMs_ = defaultTimeoutMs;
    std::array<uint32_t, bleTargetCapacity> lastSeenAtMs_{};
    std::array<bool, bleTargetCapacity> seen_{};
    std::array<bool, bleTargetCapacity> everSeen_{};
    bool started_ = false;
    bool available_ = false;
    bool scanning_ = false;
};

}  // namespace kc868a6::peripherals
