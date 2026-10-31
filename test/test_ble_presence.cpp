#include "peripherals/ble_presence.h"
#include "core/peripheral_registry.h"

#include <cassert>
#include <iostream>
#include <limits>

using namespace kc868a6;
using peripherals::BlePresence;

namespace {
class Scanner final : public peripherals::IBlePresenceScanner {
public:
    core::Result start() override { ++starts; active = startResult.ok(); return startResult; }
    void stop() override { ++stops; active = false; data = {}; }
    void setTarget(const peripherals::BleAddress *address) override {
        target = address ? *address : peripherals::BleAddress{};
        data.targetObserved = false;
    }
    core::Result poll(peripherals::BleScanSnapshot &snapshot) override {
        snapshot = data;
        data.targetObserved = false;
        return pollResult;
    }
    void observe(uint32_t nowMs) {
        data.targetObserved = true;
        data.targetSeenAtMs = nowMs;
    }
    core::Result startResult{};
    core::Result pollResult{};
    peripherals::BleScanSnapshot data{};
    peripherals::BleAddress target{};
    int starts = 0;
    int stops = 0;
    bool active = false;
};

core::PropertySnapshot value(const BlePresence &presence) {
    core::PropertySnapshot snapshot;
    assert(presence.readProperty(BlePresence::propertyId, snapshot).ok());
    assert(std::holds_alternative<bool>(snapshot.value));
    return snapshot;
}
bool inRange(const BlePresence &presence) { return std::get<bool>(value(presence).value); }

void validation() {
    Scanner scanner;
    BlePresence presence(scanner);
    peripherals::BleAddress address{1, 2, 3, 4, 5, 6};
    const auto original = address;
    for (const auto bad : {"", "AA-BB-CC-DD-EE-FF", "AA:BB:CC:DD:EE:FG",
                           " AA:BB:CC:DD:EE:FF", "AA:BB:CC:DD:EE:FF ",
                           "00:00:00:00:00:00", "FF:FF:FF:FF:FF:FF",
                           "AA:BB:CC:DD:EE"}) {
        assert(!BlePresence::parseMac(bad, address).ok());
        assert(address == original);
    }
    assert(presence.setTarget("aa:bb:cc:dd:ee:12", 1).ok());
    assert(presence.selectedMac() == "AA:BB:CC:DD:EE:12");
    std::array<char, 18> normalized{};
    assert(BlePresence::normalizeTargetMac("aa:bb:cc:dd:ee:13", normalized).ok());
    assert(std::string_view(normalized.data()) == "AA:BB:CC:DD:EE:13");
    assert(presence.selectedMac() == "AA:BB:CC:DD:EE:12");
    const auto saved = normalized;
    assert(!BlePresence::normalizeTargetMac("invalid", normalized).ok());
    assert(normalized == saved);
    assert(BlePresence::normalizeTargetMac("", normalized).ok());
    assert(normalized[0] == '\0');
    assert(!BlePresence::validateTimeoutMs(0).ok());
    assert(BlePresence::validateTimeoutMs(15000).ok());
    assert(!presence.setTarget("invalid", 2).ok());
    assert(presence.selectedMac() == "AA:BB:CC:DD:EE:12");
    assert(!presence.setTimeoutMs(999).ok());
    assert(!presence.setTimeoutMs(86400001).ok());
    assert(presence.timeoutMs() == BlePresence::defaultTimeoutMs);
    assert(presence.setTimeoutMs(1000).ok());
    assert(presence.setTimeoutMs(86400000).ok());
    assert(presence.setTarget("", 3).ok());
    assert(presence.selectedMac().empty());
    assert(value(presence).quality == core::Quality::Disabled);
}

void lifecycleAndPresence() {
    Scanner scanner;
    BlePresence presence(scanner);
    assert(presence.begin(0).ok());
    assert(scanner.starts == 0);
    assert(value(presence).quality == core::Quality::Disabled);
    assert(presence.setTarget("AA:BB:CC:DD:EE:01", 0).ok());
    assert(value(presence).quality == core::Quality::Unavailable);
    presence.setScanAvailable(true, 0);
    assert(presence.tick(0).ok());
    assert(scanner.starts == 1 && scanner.active);
    assert(!inRange(presence) && value(presence).quality == core::Quality::WarmingUp);
    assert(presence.tick(5).ok());
    assert(value(presence).quality == core::Quality::WarmingUp);
    scanner.observe(10);
    assert(presence.tick(10).ok() && inRange(presence));
    assert(value(presence).updatedAtMs == 10);
    assert(presence.tick(100).ok() && inRange(presence));
    assert(value(presence).updatedAtMs == 10);
    assert(presence.tick(15009).ok() && inRange(presence));
    scanner.observe(15009);
    assert(presence.tick(15009).ok());
    assert(presence.tick(30008).ok() && inRange(presence));
    assert(presence.tick(30009).ok() && !inRange(presence));
    scanner.observe(30010);
    assert(presence.tick(30010).ok() && inRange(presence));
    assert(presence.setTarget("aa:bb:cc:dd:ee:01", 30011).ok());
    assert(inRange(presence)); // Canonical-equivalent selection is not a change.
    scanner.observe(30011);
    assert(presence.setTarget("AA:BB:CC:DD:EE:02", 30011).ok());
    assert(!inRange(presence));
    assert(presence.tick(30012).ok() && !inRange(presence));
    assert(value(presence).quality == core::Quality::WarmingUp);
    scanner.observe(30013);
    assert(presence.tick(30013).ok() && inRange(presence));
    presence.setScanAvailable(false, 30014);
    assert(!scanner.active && scanner.stops == 1);
    assert(!inRange(presence) && value(presence).quality == core::Quality::Unavailable);
    presence.setScanAvailable(true, 30015);
    assert(presence.tick(30015).ok() && !inRange(presence));
    assert(value(presence).quality == core::Quality::WarmingUp);
    assert(scanner.starts == 2);
    presence.stop();
    assert(!scanner.active && !presence.health().ready);
    assert(value(presence).quality == core::Quality::Unavailable);
    assert(presence.begin(30016).ok());
    assert(scanner.starts == 3 && !inRange(presence));
    assert(value(presence).quality == core::Quality::WarmingUp);
}

void wrapAndFailures() {
    Scanner scanner;
    BlePresence presence(scanner);
    presence.setScanAvailable(true, 0);
    assert(presence.setTarget("AA:BB:CC:DD:EE:01", 0).ok());
    assert(presence.setTimeoutMs(1000).ok());
    assert(presence.begin(0).ok());
    const uint32_t seen = std::numeric_limits<uint32_t>::max() - 500;
    scanner.observe(seen);
    assert(presence.tick(seen).ok() && inRange(presence));
    assert(presence.tick(498).ok() && inRange(presence));
    assert(presence.tick(499).ok() && !inRange(presence));
    assert(presence.tick(seen).ok() && !inRange(presence));
    scanner.observe(501); // Callback newer than the loop's timestamp.
    assert(presence.tick(500).ok() && inRange(presence));
    assert(presence.tick(1499).ok() && inRange(presence));
    assert(presence.tick(1500).ok() && !inRange(presence));
    scanner.observe(1501);
    assert(presence.tick(1501).ok() && inRange(presence));
    assert(presence.tick(0x80001000U).ok() && !inRange(presence));
    scanner.pollResult = {core::ErrorCode::OperationFailed, "fake"};
    assert(!presence.tick(1502).ok());
    assert(!inRange(presence) && value(presence).quality == core::Quality::Unavailable);
    assert(!presence.health().ready && !scanner.active);
    scanner.pollResult = {};
    scanner.startResult = {core::ErrorCode::ResourceConflict, "fake"};
    assert(presence.tick(1503).code == core::ErrorCode::ResourceConflict);
    scanner.startResult = {};
    assert(presence.tick(1504).ok() && !inRange(presence));
    scanner.data.count = peripherals::BleScanSnapshot::capacity + 1;
    assert(presence.tick(1505).code == core::ErrorCode::CapacityExceeded);
}

void cachedUiAndRegistry() {
    Scanner scanner;
    BlePresence presence(scanner);
    peripherals::BleScanSnapshot results;
    assert(!presence.scanResults(0, results).ok());
    core::PeripheralRegistry registry;
    assert(registry.add({&presence, &presence, &presence, nullptr}).ok());
    assert(registry.findTrigger("ble.presence/enter"));
    assert(registry.findTrigger("ble.presence/leave"));
    presence.setScanAvailable(true, 0);
    assert(registry.begin(0).ok());
    assert(value(presence).quality == core::Quality::Disabled);
    scanner.data.count = peripherals::BleScanSnapshot::capacity;
    scanner.data.devices[0].seenAtMs = 1;
    scanner.data.devices[1].seenAtMs = 29999;
    assert(registry.tick(30000).ok());
    assert(presence.scanResults(30000, results).ok());
    assert(results.count == 2);
    assert(presence.scanResults(30001, results).ok() && results.count == 1);
    assert(scanner.starts == 1);
    assert(presence.setTarget("AA:BB:CC:DD:EE:01", 30002).ok());
    scanner.observe(30002); // Target mailbox remains independent of a full UI cache.
    assert(registry.tick(30002).ok() && inRange(presence));
    core::PropertySnapshot snapshot;
    assert(registry.readProperty(BlePresence::propertyId, 999999, snapshot).ok());
    assert(snapshot.quality == core::Quality::Valid); // No descriptor staleness.
    assert(presence.readProperty("missing", snapshot).code == core::ErrorCode::NotFound);
    registry.stop();
}

void initialLeaveSuppression() {
    Scanner scanner;
    BlePresence presence(scanner);
    assert(presence.setTarget("AA:BB:CC:DD:EE:01", 0).ok());
    assert(presence.setTimeoutMs(1000).ok());
    presence.setScanAvailable(true, 0);
    assert(presence.begin(0).ok());
    const auto leaveMatches = [&presence] {
        return value(presence).quality == core::Quality::Valid && !inRange(presence);
    };
    assert(!leaveMatches());
    assert(presence.tick(5000).ok());
    assert(!leaveMatches());
    scanner.observe(5010);
    assert(presence.tick(5010).ok() && inRange(presence));
    assert(!leaveMatches());
    assert(presence.tick(6010).ok());
    assert(leaveMatches());
    presence.setScanAvailable(false, 6020);
    assert(!leaveMatches());
    presence.setScanAvailable(true, 6030);
    assert(presence.tick(6030).ok());
    assert(!leaveMatches());
    assert(presence.tick(10000).ok());
    assert(!leaveMatches());
    scanner.observe(10001);
    assert(presence.tick(10001).ok() && inRange(presence));
    assert(presence.tick(11001).ok() && leaveMatches());
    assert(presence.setTarget("AA:BB:CC:DD:EE:02", 11002).ok());
    assert(presence.tick(11002).ok() && !leaveMatches());
}
}

void runBlePresenceTests() {
    validation();
    lifecycleAndPresence();
    wrapAndFailures();
    cachedUiAndRegistry();
    initialLeaveSuppression();
    std::cout << "BLE presence tests passed\n";
}

#ifndef KC868_BLE_PRESENCE_NO_MAIN
int main() {
    runBlePresenceTests();
}
#endif
