#include "peripherals/ble_targets.h"

namespace kc868a6::peripherals {
core::Result readBleTargets(JsonVariantConst source, BleTargets &targets) {
    if (!source.is<JsonObjectConst>() || source.size() != (source.containsKey("legacyAddress") ? 3U : 2U) ||
        !source["schemaVersion"].is<unsigned>() || !source["addresses"].is<JsonArrayConst>())
        return {core::ErrorCode::InvalidArgument, "ble_targets"};
    if (source["schemaVersion"].as<unsigned>() != 1)
        return {core::ErrorCode::UnsupportedVersion, "ble_targets"};
    const auto addresses = source["addresses"].as<JsonArrayConst>();
    if (addresses.size() > bleTargetCapacity)
        return {core::ErrorCode::CapacityExceeded, "ble_targets"};
    BleTargets candidate;
    if (source.containsKey("legacyAddress")) {
        if (!source["legacyAddress"].is<const char *>()) return {core::ErrorCode::InvalidArgument, "ble_targets"};
        const JsonString mac = source["legacyAddress"].as<JsonString>();
        if (mac.size() != 0) {
            const auto valid = BlePresence::parseMac({mac.c_str(), mac.size()}, candidate.legacyAddress);
            if (!valid.ok()) return valid;
        }
    }
    for (JsonVariantConst address : addresses) {
        if (!address.is<const char *>()) return {core::ErrorCode::InvalidArgument, "ble_targets"};
        const JsonString mac = address.as<JsonString>();
        const auto valid = BlePresence::parseMac({mac.c_str(), mac.size()}, candidate.addresses[candidate.count]);
        if (!valid.ok()) return valid;
        for (size_t i = 0; i < candidate.count; ++i) if (candidate.addresses[i] == candidate.addresses[candidate.count])
            return {core::ErrorCode::DuplicateId, "ble_targets"};
        ++candidate.count;
    }
    targets = candidate;
    return {};
}
core::Result writeBleTargets(JsonObject destination, const BleTargets &targets) {
    if (targets.count > bleTargetCapacity) return {core::ErrorCode::CapacityExceeded, "ble_targets"};
    destination["schemaVersion"] = 1;
    const auto legacy = BlePresence::formatMac(targets.legacyAddress);
    destination["legacyAddress"].set(targets.legacyAddress == BleAddress{} ?
        std::string_view{""} : std::string_view{legacy.data(), 17});
    auto addresses = destination.createNestedArray("addresses");
    for (size_t i = 0; i < targets.count; ++i) {
        const auto mac = BlePresence::formatMac(targets.addresses[i]);
        addresses.add(std::string_view{mac.data(), 17});
    }
    return {};
}
}
