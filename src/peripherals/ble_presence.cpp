#include "peripherals/ble_presence.h"

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"ble.presence", "ble-presence"};
const std::array<core::PropertyDescriptor, 1> propertyDescriptors{{
    {BlePresence::propertyId, core::ValueType::Boolean, ""}
}};
// Both use the existing property evaluator: enter == true, leave == false.
// These are conditions, not unsupported property-backed Event descriptors.
const std::array<core::TriggerDescriptor, 2> triggerDescriptors{{
    {"ble.presence/enter", core::TriggerKind::Condition, BlePresence::propertyId},
    {"ble.presence/leave", core::TriggerKind::Condition, BlePresence::propertyId}
}};
int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
}

const core::PeripheralDescriptor &BlePresence::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> BlePresence::properties() const { return propertyDescriptors; }
core::List<core::TriggerDescriptor> BlePresence::triggers() const { return triggerDescriptors; }

core::Result BlePresence::parseMac(std::string_view text, BleAddress &address) {
    if (text.size() != 17) return {core::ErrorCode::InvalidArgument, "ble_presence_mac"};
    BleAddress parsed{};
    bool allZero = true;
    bool allOnes = true;
    for (size_t i = 0; i < parsed.size(); ++i) {
        const int high = hexDigit(text[i * 3]);
        const int low = hexDigit(text[i * 3 + 1]);
        if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != ':')) {
            return {core::ErrorCode::InvalidArgument, "ble_presence_mac"};
        }
        parsed[i] = static_cast<uint8_t>(high * 16 + low);
        allZero = allZero && parsed[i] == 0;
        allOnes = allOnes && parsed[i] == 255;
    }
    if (allZero || allOnes) return {core::ErrorCode::InvalidArgument, "ble_presence_mac"};
    address = parsed;
    return {};
}

std::array<char, 18> BlePresence::formatMac(const BleAddress &address) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::array<char, 18> text{};
    for (size_t i = 0; i < address.size(); ++i) {
        text[i * 3] = digits[address[i] >> 4];
        text[i * 3 + 1] = digits[address[i] & 15];
        if (i < 5) text[i * 3 + 2] = ':';
    }
    return text;
}

core::Result BlePresence::normalizeTargetMac(std::string_view text,
                                             std::array<char, 18> &normalized) {
    BleAddress address{};
    if (!text.empty()) {
        const auto result = parseMac(text, address);
        if (!result.ok()) return result;
    }
    normalized = text.empty() ? std::array<char, 18>{} : formatMac(address);
    return {};
}

core::Result BlePresence::validateTimeoutMs(uint32_t timeoutMs) {
    if (timeoutMs < minimumTimeoutMs || timeoutMs > maximumTimeoutMs) {
        return {core::ErrorCode::InvalidArgument, "ble_presence_timeout_ms"};
    }
    return {};
}

void BlePresence::setValue(bool inRange, core::Quality quality, uint32_t nowMs,
                           core::ErrorCode error) {
    if (std::get<bool>(value_.value) != inRange || value_.quality != quality ||
        value_.error != error) {
        value_ = {inRange, quality, nowMs, error};
    }
}

void BlePresence::resetPresence(uint32_t nowMs) {
    seen_ = false;
    everSeen_ = false;
    lastSeenAtMs_ = 0;
    value_ = {false, !hasTarget_ ? core::Quality::Disabled
                               : started_ && available_ ? core::Quality::WarmingUp
                                                        : core::Quality::Unavailable,
              nowMs};
}

core::Result BlePresence::setTarget(std::string_view mac, uint32_t nowMs) {
    std::array<char, 18> normalized{};
    const auto result = normalizeTargetMac(mac, normalized);
    if (!result.ok()) return result;
    BleAddress address{};
    if (!mac.empty()) {
        parseMac(mac, address);
    }
    if (hasTarget_ == !mac.empty() && (!hasTarget_ || target_ == address)) return {};
    hasTarget_ = !mac.empty();
    target_ = address;
    selectedMac_ = normalized;
    scanner_.setTarget(hasTarget_ ? &target_ : nullptr);
    resetPresence(nowMs);
    return {};
}

core::Result BlePresence::setTimeoutMs(uint32_t timeoutMs) {
    const auto result = validateTimeoutMs(timeoutMs);
    if (!result.ok()) return result;
    timeoutMs_ = timeoutMs;
    return {};
}

void BlePresence::setScanAvailable(bool available, uint32_t nowMs) {
    if (available_ == available) return;
    if (scanning_) scanner_.stop();
    scanning_ = false;
    available_ = available;
    scan_ = {};
    health_ = {false, core::ErrorCode::None};
    resetPresence(nowMs);
}

core::Result BlePresence::begin(uint32_t nowMs) {
    started_ = true;
    scanner_.setTarget(hasTarget_ ? &target_ : nullptr);
    resetPresence(nowMs);
    return tick(nowMs);
}

core::Result BlePresence::tick(uint32_t nowMs) {
    if (!started_ || !available_) {
        health_ = {false, core::ErrorCode::None};
        setValue(false, hasTarget_ ? core::Quality::Unavailable : core::Quality::Disabled, nowMs);
        return {};
    }
    core::Result result;
    if (!scanning_) {
        result = scanner_.start();
        scanning_ = result.ok();
    }
    if (result.ok()) result = scanner_.poll(scan_);
    if (result.ok() && scan_.count > BleScanSnapshot::capacity) {
        result = {core::ErrorCode::CapacityExceeded, "ble_scan_results"};
    }
    if (!result.ok()) {
        if (scanning_) scanner_.stop();
        scanning_ = false;
        seen_ = false;
        everSeen_ = false;
        scan_ = {};
        health_ = {false, result.code};
        setValue(false, hasTarget_ ? core::Quality::Unavailable : core::Quality::Disabled,
                 nowMs, hasTarget_ ? result.code : core::ErrorCode::None);
        return result;
    }
    health_ = {true, core::ErrorCode::None};
    if (!hasTarget_) {
        setValue(false, core::Quality::Disabled, nowMs);
        return {};
    }
    if (scan_.targetObserved) {
        seen_ = true;
        everSeen_ = true;
        lastSeenAtMs_ = scan_.targetSeenAtMs;
        // A callback during poll can be slightly newer than the loop's nowMs.
        if (static_cast<int32_t>(nowMs - lastSeenAtMs_) < 0) lastSeenAtMs_ = nowMs;
    }
    if (seen_ && static_cast<uint32_t>(nowMs - lastSeenAtMs_) >= timeoutMs_) {
        seen_ = false; // Do not resurrect expired sightings on a later millis wrap.
    }
    setValue(seen_, everSeen_ ? core::Quality::Valid : core::Quality::WarmingUp, nowMs);
    return {};
}

void BlePresence::stop() {
    if (scanning_) scanner_.stop();
    scanning_ = false;
    started_ = false;
    scan_ = {};
    health_ = {};
    resetPresence(value_.updatedAtMs);
}

core::Result BlePresence::readProperty(std::string_view id,
                                       core::PropertySnapshot &snapshot) const {
    if (id != propertyId) return {core::ErrorCode::NotFound, id};
    snapshot = value_;
    return {};
}

core::Result BlePresence::scanResults(uint32_t nowMs, BleScanSnapshot &snapshot) const {
    snapshot = {};
    if (!started_ || !available_ || !scanning_ || !health_.ready) {
        return {core::ErrorCode::Unavailable, "ble_scan_disabled"};
    }
    for (size_t i = 0; i < scan_.count; ++i) {
        if (static_cast<uint32_t>(nowMs - scan_.devices[i].seenAtMs) < scanResultLifetimeMs) {
            snapshot.devices[snapshot.count++] = scan_.devices[i];
        }
    }
    return {};
}

}  // namespace kc868a6::peripherals
