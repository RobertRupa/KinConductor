#include "peripherals/current_inputs.h"
#include "peripherals/analog_inputs.h"
#include "config/network_config.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

using namespace kc868a6;
namespace {
class TestAdc final : public peripherals::IAnalogInputHardware {
public:
    uint16_t raw = 2000;
    bool fail = false;
    core::Result begin() override { return {}; }
    core::Result read(uint8_t, uint16_t &value) override {
        value = raw;
        return fail ? core::Result{core::ErrorCode::OperationFailed, "adc"} : core::Result{};
    }
};
void testCalibration() {
    peripherals::CurrentConfig config;
    auto &channel = config.channels[0];
    channel.signal = peripherals::CurrentSignal::CurrentLoop;
    channel.calibrated = true;
    channel.rawLow = 1000;
    channel.rawHigh = 3000;
    double signal = -1, current = -1;
    assert(peripherals::convertCurrent(channel, 1000, signal, current).ok());
    assert(signal == 4 && current == 0);
    assert(peripherals::convertCurrent(channel, 2000, signal, current).ok());
    assert(signal == 12 && current == 10);
    assert(peripherals::convertCurrent(channel, 3000, signal, current).ok());
    assert(signal == 20 && current == 20);
    assert(!peripherals::convertCurrent(channel, 999, signal, current).ok());
    assert(signal == 20 && current == 20);
    channel.rawHigh = channel.rawLow;
    assert(!peripherals::validateCurrentConfig(config).ok());
    channel.rawHigh = 3000;
    channel.signalHigh = NAN;
    assert(!peripherals::validateCurrentConfig(config).ok());
    channel.signalHigh = 20;
    channel.signal = peripherals::CurrentSignal::Voltage;
    assert(!peripherals::validateCurrentConfig(config).ok());
    channel.signalLow = 0; channel.signalHigh = 5;
    assert(peripherals::convertCurrent(channel, 2000, signal, current).ok() && signal == 2.5);
    DynamicJsonDocument document(peripherals::currentJsonCapacity);
    assert(peripherals::writeCurrentConfig(document.to<JsonObject>(), config).ok());
    peripherals::CurrentConfig decoded;
    assert(peripherals::readCurrentConfig(document.as<JsonVariantConst>(), decoded).ok());
    assert(decoded.channels[0].signal == peripherals::CurrentSignal::Voltage);
    assert(decoded.channels[0].rawLow == 1000);
    document["channels"][0]["rawLow"] = 3000;
    assert(!peripherals::readCurrentConfig(document.as<JsonVariantConst>(), decoded).ok());
    assert(decoded.channels[0].rawLow == 1000);
    document["channels"][0]["rawLow"] = 1000;
    document["channels"][0]["calibrated"] = 1;
    assert(!peripherals::readCurrentConfig(document.as<JsonVariantConst>(), decoded).ok());
    document["channels"][0]["calibrated"] = true;
    document["channels"][0]["signal"] = 3;
    assert(!peripherals::readCurrentConfig(document.as<JsonVariantConst>(), decoded).ok());
    StaticJsonDocument<32> tiny;
    assert(!peripherals::writeCurrentConfig(tiny.to<JsonObject>(), config).ok());
}
void testCurrentModule() {
    TestAdc hardware;
    peripherals::AnalogInputs adc{hardware, {36,39,34,35}};
    core::PeripheralRegistry registry;
    peripherals::CurrentInputs current{registry};
    assert(registry.add({&adc, &adc, &adc, nullptr}).ok());
    assert(registry.add({&current, &current, &current, nullptr}).ok());
    assert(registry.begin(0).ok());
    assert(registry.tick(10).ok());
    core::PropertySnapshot snapshot;
    assert(registry.readProperty("current.main/channel/1/amperes", 10, snapshot).ok());
    assert(snapshot.quality == core::Quality::Disabled);
    peripherals::CurrentConfig config;
    config.channels[0].signal = peripherals::CurrentSignal::CurrentLoop;
    config.channels[0].rawLow = 1000; config.channels[0].rawHigh = 3000;
    assert(current.configure(config).ok());
    assert(registry.tick(20).ok());
    assert(registry.readProperty("current.main/channel/1/amperes", 20, snapshot).ok());
    assert(snapshot.quality == core::Quality::WarmingUp);
    config.channels[0].calibrated = true;
    assert(current.configure(config).ok());
    assert(registry.tick(30).ok());
    assert(registry.readProperty("current.main/channel/1/amperes", 30, snapshot).ok());
    assert(snapshot.quality == core::Quality::Valid && std::get<double>(snapshot.value) == 10);
    assert(snapshot.updatedAtMs == 30);
    hardware.raw = 500;
    assert(!registry.tick(40).ok());
    assert(registry.readProperty("current.main/channel/1/amperes", 40, snapshot).ok());
    assert(snapshot.quality == core::Quality::Error);
    hardware.raw = 2000;
    assert(registry.tick(50).ok());
    hardware.fail = true;
    assert(!registry.tick(60).ok());
    assert(registry.readProperty("current.main/channel/1/amperes", 60, snapshot).ok());
    assert(snapshot.quality == core::Quality::Error);
    current.unavailable(core::ErrorCode::StorageWriteFailed);
    assert(!current.tick(70).ok());
    assert(current.configure(config).ok());
    hardware.fail = false;
    assert(registry.tick(80).ok());
}
void testHostname() {
    config::NetworkConfig hostname;
    assert(std::strcmp(hostname.hostname.data(), "kinconductor") == 0);
    assert(config::normalizeHostname("My-Controller", hostname).ok());
    assert(std::strcmp(hostname.hostname.data(), "my-controller") == 0);
    for (const char *invalid : {"", "-bad", "bad-", "bad.local", "with space", "bad_name"}) {
        assert(!config::normalizeHostname(invalid, hostname).ok());
        assert(std::strcmp(hostname.hostname.data(), "my-controller") == 0);
    }
    assert(!config::normalizeHostname(std::string_view("a\0b",3), hostname).ok());
    assert(config::normalizeHostname(std::string(63, 'a'), hostname).ok());
    assert(!config::normalizeHostname(std::string(64, 'a'), hostname).ok());
    assert(hostname.hostname[62] == 'a' && hostname.hostname[63] == 0);
    DynamicJsonDocument doc(512);
    assert(config::writeNetworkConfig(doc.to<JsonObject>(), hostname).ok());
    config::NetworkConfig decoded;
    assert(config::readNetworkConfig(doc.as<JsonVariantConst>(), decoded).ok());
    assert(decoded.hostname == hostname.hostname);
    doc["hostname"] = "too.long.local";
    assert(!config::readNetworkConfig(doc.as<JsonVariantConst>(), decoded).ok());
    doc["hostname"] = "valid"; doc["schemaVersion"] = 2;
    assert(!config::readNetworkConfig(doc.as<JsonVariantConst>(), decoded).ok());
}
}
void runMeasurementTests() {
    testCalibration(); testCurrentModule(); testHostname();
    std::cout << "PASS: 3 measurement/network groups (two-point scaling, current quality, hostname)\n";
}
