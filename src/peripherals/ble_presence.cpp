#include "peripherals/ble_presence.h"
#include <cstdio>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"ble.presence", "ble-presence"};
int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
}

const core::PeripheralDescriptor &BlePresence::descriptor() const { return identity; }
BlePresence::BlePresence(IBlePresenceScanner &scanner) : scanner_(scanner) {
    properties_[0] = {propertyId, core::ValueType::Boolean, ""};
    triggers_[0] = {"ble.presence/enter", core::TriggerKind::Condition, propertyId};
    triggers_[1] = {"ble.presence/leave", core::TriggerKind::Condition, propertyId};
}
core::List<core::PropertyDescriptor> BlePresence::properties() const { return {properties_.data(), targets_.count + 1}; }
core::List<core::TriggerDescriptor> BlePresence::triggers() const { return {triggers_.data(), targets_.count + 2}; }

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

void BlePresence::setValue(size_t index, bool inRange, core::Quality quality, uint32_t nowMs,
                           core::ErrorCode error) {
    auto &value = values_[index];
    if (std::get<bool>(value.value) != inRange || value.quality != quality ||
        value.error != error) {
        value = {inRange, quality, nowMs, error};
    }
}

void BlePresence::resetPresence(uint32_t nowMs) {
    seen_ = {};
    everSeen_ = {};
    lastSeenAtMs_ = {};
    for (auto &value : values_) value = {false, started_ && available_ ?
        core::Quality::WarmingUp : core::Quality::Unavailable, nowMs};
    disabled_ = {false, core::Quality::Disabled, nowMs};
}

core::Result BlePresence::setTarget(std::string_view mac, uint32_t nowMs) {
    std::array<char, 18> normalized{};
    const auto result = normalizeTargetMac(mac, normalized);
    if (!result.ok()) return result;
    BleAddress address{};
    if (!mac.empty()) {
        parseMac(mac, address);
    }
    BleTargets targets;
    if (!mac.empty()) { targets.addresses[0] = address; targets.count = 1; targets.legacyAddress = address; }
    return setTargets(targets, nowMs);
}

core::Result BlePresence::setTargets(const BleTargets &targets, uint32_t nowMs) {
    if (targets.count > bleTargetCapacity) return {core::ErrorCode::CapacityExceeded, "ble_targets"};
    if (targets.legacyAddress != BleAddress{}) {
        BleAddress parsed;
        const auto mac = formatMac(targets.legacyAddress);
        const auto valid = parseMac(mac.data(), parsed);
        if (!valid.ok()) return valid;
    }
    for (size_t i = 0; i < targets.count; ++i) {
        BleAddress parsed;
        const auto mac = formatMac(targets.addresses[i]);
        const auto valid = parseMac(mac.data(), parsed);
        if (!valid.ok()) return valid;
        for (size_t j = 0; j < i; ++j) if (targets.addresses[j] == parsed)
            return {core::ErrorCode::DuplicateId, "ble_targets"};
    }
    if (targets.count == targets_.count && targets.addresses == targets_.addresses &&
        targets.legacyAddress == targets_.legacyAddress) return {};
    const auto previous = targets_;
    const auto oldValues = values_;
    const auto oldSeen = seen_;
    const auto oldEverSeen = everSeen_;
    const auto oldLastSeen = lastSeenAtMs_;
    targets_ = targets;
    legacyMac_ = targets.legacyAddress == BleAddress{} ? std::array<char, 18>{} : formatMac(targets.legacyAddress);
    resetPresence(nowMs);
    for (size_t i = 0; i < targets.count; ++i) {
        macs_[i] = formatMac(targets.addresses[i]);
        std::snprintf(propertyIds_[i].data(), propertyIds_[i].size(), "ble.presence/%s/in_range", macs_[i].data());
        std::snprintf(triggerIds_[i].data(), triggerIds_[i].size(), "ble.presence/%s/state", macs_[i].data());
        properties_[i + 1] = {propertyIds_[i].data(), core::ValueType::Boolean, ""};
        triggers_[i + 2] = {triggerIds_[i].data(), core::TriggerKind::Condition, propertyIds_[i].data()};
        for (size_t j = 0; j < previous.count; ++j) if (previous.addresses[j] == targets.addresses[i]) {
            values_[i] = oldValues[j]; seen_[i] = oldSeen[j];
            everSeen_[i] = oldEverSeen[j]; lastSeenAtMs_[i] = oldLastSeen[j];
        }
    }
    scanner_.setTargets(targets_);
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
    scanner_.setTargets(targets_);
    resetPresence(nowMs);
    return tick(nowMs);
}

core::Result BlePresence::tick(uint32_t nowMs) {
    if (!started_ || !available_) {
        health_ = {false, core::ErrorCode::None};
        for (size_t i = 0; i < targets_.count; ++i) setValue(i, false, core::Quality::Unavailable, nowMs);
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
        seen_ = {};
        everSeen_ = {};
        scan_ = {};
        health_ = {false, result.code};
        for (size_t i = 0; i < targets_.count; ++i) setValue(i, false, core::Quality::Unavailable, nowMs, result.code);
        return result;
    }
    health_ = {true, core::ErrorCode::None};
    for (size_t i = 0; i < targets_.count; ++i) {
        if (scan_.targetObserved[i]) {
            seen_[i] = true;
            everSeen_[i] = true;
            lastSeenAtMs_[i] = scan_.targetSeenAtMs[i];
            // A callback during poll can be slightly newer than the loop's nowMs.
            if (static_cast<int32_t>(nowMs - lastSeenAtMs_[i]) < 0) lastSeenAtMs_[i] = nowMs;
        }
        if (seen_[i] && static_cast<uint32_t>(nowMs - lastSeenAtMs_[i]) >= timeoutMs_) {
            seen_[i] = false; // Do not resurrect expired sightings on a later millis wrap.
        }
        setValue(i, seen_[i], everSeen_[i] ? core::Quality::Valid : core::Quality::WarmingUp, nowMs);
    }
    return {};
}

void BlePresence::stop() {
    if (scanning_) scanner_.stop();
    scanning_ = false;
    started_ = false;
    scan_ = {};
    health_ = {};
    resetPresence(disabled_.updatedAtMs);
}

core::Result BlePresence::readProperty(std::string_view id,
                                       core::PropertySnapshot &snapshot) const {
    if (id == propertyId) {
        for (size_t i = 0; i < targets_.count; ++i) if (targets_.addresses[i] == targets_.legacyAddress) {
            snapshot = values_[i]; return {};
        }
        snapshot = disabled_; return {};
    }
    for (size_t i = 0; i < targets_.count; ++i) if (id == properties_[i + 1].id) {
        snapshot = values_[i]; return {};
    }
    return {core::ErrorCode::NotFound, id};
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
