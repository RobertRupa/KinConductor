#include "config/network_config.h"

namespace kc868a6::config {
core::Result normalizeHostname(std::string_view source, NetworkConfig &destination) {
    const auto invalid = core::Result{core::ErrorCode::InvalidArgument, "hostname"};
    if (source.empty() || source.size() > 63 || source.front() == '-' || source.back() == '-') return invalid;
    NetworkConfig next;
    next.hostname.fill(0);
    for (size_t index = 0; index < source.size(); ++index) {
        char c = source[index];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-') return invalid;
        next.hostname[index] = c;
    }
    destination = next;
    return {};
}
core::Result readNetworkConfig(JsonVariantConst source, NetworkConfig &destination) {
    if (!source.is<JsonObjectConst>() || source.size() != 2 ||
        !source["schemaVersion"].is<uint32_t>() || source["schemaVersion"].as<uint32_t>() != 1 ||
        !source["hostname"].is<const char *>()) {
        return {core::ErrorCode::InvalidArgument, "network_config"};
    }
    const JsonString name = source["hostname"].as<JsonString>();
    return normalizeHostname({name.c_str(), name.size()}, destination);
}
core::Result writeNetworkConfig(JsonObject destination, const NetworkConfig &config) {
    return destination["schemaVersion"].set(1) && destination["hostname"].set(config.hostname.data())
        ? core::Result{} : core::Result{core::ErrorCode::CapacityExceeded, "network_json"};
}
}
