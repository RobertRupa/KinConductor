#include "web/ble_scan.h"
namespace kc868a6::web {
core::Result writeBleScan(JsonObject destination, const peripherals::BleScanSnapshot &scan) {
    if (scan.count > peripherals::BleScanSnapshot::capacity) {
        return {core::ErrorCode::CapacityExceeded, "ble_scan_count"};
    }
    auto devices = destination.createNestedArray("devices");
    bool ok = !devices.isNull();
    for (size_t index = 0; index < scan.count && ok; ++index) {
        const auto &item = scan.devices[index];
        auto device = devices.createNestedObject();
        const auto mac = peripherals::BlePresence::formatMac(item.address);
        ok = device["name"].set(item.name[0] ? item.name.data() : "(unnamed)") &&
            device["address"].set(std::string_view{mac.data(), 17}) &&
            device["rssi"].set(item.rssi) && device["addressType"].set(item.addressType);
    }
    return ok ? core::Result{} : core::Result{core::ErrorCode::CapacityExceeded, "ble_scan_json"};
}
}
