#include "peripherals/board_io.h"
#include "peripherals/analog_inputs.h"
#include "web/capabilities.h"
#include "config/settings_json.h"
#include "display/screen_layout.h"
#include "peripherals/sensor_readings.h"
#include "peripherals/fan_control.h"
#include "core/ifttt_condition.h"
#include "peripherals/ble_presence.h"
#include "peripherals/transport.h"
#include "peripherals/current_inputs.h"
#include "peripherals/udp_rules.h"
#include "web/ble_scan.h"
#include <cassert>
#include <iostream>
#include <cstring>
#include <cmath>

using namespace kc868a6;
namespace {
class FakeIo final : public peripherals::IBoardIoHardware {
public:
    uint8_t inputs = 0xFE;
    uint8_t relays = 0xFF;
    bool fail = false;
    unsigned writes = 0;
    core::Result readInputs(uint8_t &bits) override {
        bits = inputs;
        return fail ? core::Result{core::ErrorCode::OperationFailed, "inputs"} : core::Result{};
    }
    core::Result readRelays(uint8_t &bits) override { bits = relays; return {}; }
    core::ActionResult writeRelay(uint8_t channel, bool on) override {
        ++writes;
        if (on) relays &= ~(1U << channel);
        else relays |= (1U << channel);
        return {{}, true, true};
    }
};
class FakeAnalog final : public peripherals::IAnalogInputHardware {
public:
    core::Result begin() override { return {}; }
    core::Result read(uint8_t, uint16_t &value) override { value = 4095; return {}; }
};
class FakeFan final : public peripherals::IFanHardware {
public:
    bool enabled = false;
    bool fail = false;
    uint8_t duty = 0;
    unsigned writes = 0;
    core::Result configure(bool on, uint8_t, uint16_t) override {
        enabled = on; duty = 0; return {};
    }
    core::Result write(uint8_t value) override {
        if (fail) return {core::ErrorCode::OperationFailed, "fan"};
        duty = value; ++writes; return {};
    }
};
class FakeBle final : public peripherals::IBlePresenceScanner {
public:
    core::Result start() override { return {}; }
    void stop() override {}
    void setTargets(const peripherals::BleTargets &) override {}
    core::Result poll(peripherals::BleScanSnapshot &scan) override { scan = {}; return {}; }
};
class FakeRuleUdp final : public peripherals::IUdpRulesHardware {
public:
    bool connected() const override { return true; }
    core::Result open(uint8_t, uint16_t) override { return {}; }
    void close(uint8_t) override {}
    core::Result receive(uint8_t, char *, size_t, size_t &size,
                        std::array<uint8_t, 4> &, uint16_t &, bool &received) override {
        size = 0; received = false; return {};
    }
};
class FakeTransport final : public peripherals::ITransportHardware {
public:
    bool wifiConnected() const override { return true; }
    core::Result listenUdp(uint16_t) override { return {}; }
    void stopUdp() override {}
    core::Result receiveUdp(char *, size_t, size_t &size, bool &received) override {
        size = 0; received = false; return {};
    }
    core::ActionResult sendUdp(std::string_view, uint16_t, std::string_view) override { return {{}, true, false}; }
    core::Result serialRead(uint8_t, int &byte) override { byte = -1; return {}; }
    core::ActionResult serialSend(uint8_t, std::string_view) override { return {{}, true, false}; }
};
void testFanAndConditions() {
    FakeFan hardware;
    peripherals::FanControl fan{hardware};
    core::PeripheralRegistry registry;
    assert(registry.add({&fan, &fan, &fan, &fan}).ok());
    assert(registry.begin(0).ok());
    const std::array<core::Parameter, 1> duty{{{"percent", uint32_t{75}}}};
    assert(registry.execute("fan.main/set", duty).result.code == core::ErrorCode::Unavailable);
    assert(fan.configure(true, 25, 1000, 10).ok());
    const auto result = registry.execute("fan.main/set", duty);
    assert(result.result.ok() && result.applied && !result.confirmed && hardware.duty == 75);
    const std::array<core::Parameter, 1> invalid{{{"percent", uint32_t{101}}}};
    assert(!registry.execute("fan.main/set", invalid).result.ok() && hardware.writes == 1);
    hardware.fail = true;
    assert(!registry.execute("fan.main/set", duty).result.ok());
    assert(fan.configure(false, 25, 1000, 20).ok() && hardware.duty == 0);
    assert(!fan.configure(true, 24, 1000, 30).ok());
    core::ConditionState event;
    assert(!event.fires(0, 0, "changed"));
    assert(event.fires(1, 0, "changed"));
    assert(event.fires(2, 0, "changed"));
    assert(!event.fires(2, 0, "changed"));
    assert(!event.fires(NAN, 0, "changed"));
    assert(!event.fires(3, 0, "changed"));
    assert(event.fires(4, 0, "changed"));
    core::ConditionState presence;
    assert(!presence.fires(NAN, 1, "=="));
    assert(presence.fires(1, 1, "=="));
    assert(!presence.fires(1, 1, "=="));
    assert(!presence.fires(0, 1, "=="));
    assert(presence.fires(1, 1, "=="));
}
void testIoAndActions() {
    FakeIo hardware;
    peripherals::BoardIo io{hardware};
    peripherals::SensorReadings sensors;
    core::PeripheralRegistry registry;
    assert(registry.add({&io, &io, &io, &io}).ok());
    assert(registry.add({&sensors, &sensors, &sensors, nullptr}).ok());
    assert(registry.binding(0) && !registry.binding(2));
    assert(registry.begin(0).ok());
    assert(registry.tick(20).ok());
    core::PropertySnapshot value;
    assert(registry.readProperty("io.main/input/1", 20, value).ok());
    assert(std::get<bool>(value.value) && value.updatedAtMs == 20);
    DynamicJsonDocument request(2048);
    assert(!deserializeJson(request, R"({"id":"io.main/relay/set","parameters":{"channel":6,"on":true}})"));
    assert(web::executeJsonAction(registry, request.as<JsonVariantConst>(), false).result.ok());
    assert(hardware.writes == 0);
    const auto action = web::executeJsonAction(registry, request.as<JsonVariantConst>());
    assert(action.result.ok() && action.applied && action.confirmed && hardware.relays == 0xDF);
    const char *invalid[] = {
        R"({"id":"io.main/relay/set","parameters":{"channel":0,"on":true}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":7,"on":true}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":4294967296,"on":true}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":1.5,"on":true}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":1,"on":1}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":1}})",
        R"({"id":"io.main/relay/set","parameters":{"channel":1,"on":true,"extra":1}})",
        R"({"id":"missing","parameters":{}})",
        "[]",
    };
    for (auto json : invalid) {
        request.clear(); assert(!deserializeJson(request, json));
        assert(!web::executeJsonAction(registry, request.as<JsonVariantConst>()).result.ok());
        assert(hardware.writes == 1);
    }
    hardware.fail = true;
    assert(!registry.tick(40).ok());
    assert(registry.readProperty("io.main/input/1", 40, value).ok());
    assert(value.quality == core::Quality::Error);
    hardware.fail = false;
    assert(registry.tick(60).ok());
    assert(registry.readProperty("io.main/input/1", 60, value).ok());
    assert(value.quality == core::Quality::Valid);
}
void testCapabilityJson() {
    FakeIo hardware;
    peripherals::BoardIo io{hardware};
    peripherals::SensorReadings sensors;
    FakeAnalog adcHardware;
    peripherals::AnalogInputs adc{adcHardware, {36, 39, 34, 35}};
    FakeFan fanHardware;
    peripherals::FanControl fan{fanHardware};
    FakeBle scanner;
    peripherals::BlePresence presence{scanner};
    FakeTransport transportHardware;
    peripherals::Transport transport{transportHardware};
    core::PeripheralRegistry registry;
    assert(registry.add({&io, &io, &io, &io}).ok());
    assert(registry.add({&sensors, &sensors, &sensors, nullptr}).ok());
    assert(registry.add({&adc, &adc, &adc, nullptr}).ok());
    assert(registry.begin(0).ok()); assert(registry.tick(25).ok());
    assert(sensors.publish(0, 23.5, core::Quality::Valid, 20).ok());
    assert(sensors.publish(1, 0.0, core::Quality::Disabled, 20).ok());
    DynamicJsonDocument document(web::capabilitiesJsonCapacity);
    assert(web::writeCapabilities(document.to<JsonObject>(), registry, 25).ok());
    assert(!document.overflowed());
    assert(document["schemaVersion"] == 1 && document["revision"] == 3);
    assert(document["modules"].size() == 3);
    assert(document["modules"][0]["properties"].size() == 14);
    assert(document["modules"][0]["properties"][0]["value"].as<bool>());
    assert(document["modules"][0]["triggers"].size() == 14);
    assert(document["modules"][0]["actions"][0]["parameters"].size() == 2);
    assert(document["modules"][1]["properties"].size() == 16);
    assert(document["modules"][1]["properties"][0]["value"].as<double>() == 23.5);
    assert(document["modules"][1]["properties"][0]["updatedAtMs"] == 20);
    assert(document["modules"][1]["properties"][1]["quality"] == "disabled");
    assert(document["modules"][2]["properties"][0]["value"] == 4095);
    assert(!sensors.publish(16, 0.0, core::Quality::Valid, 20).ok());
    assert(!sensors.publish(0, NAN, core::Quality::Valid, 40).ok());
    hardware.fail = true; assert(!registry.tick(30).ok());
    document.clear();
    assert(web::writeCapabilities(document.to<JsonObject>(), registry, 30).ok());
    assert(document["modules"][0]["properties"][0]["value"].isNull());
    assert(document["modules"][0]["properties"][0]["quality"] == "error");
    assert(document["modules"][1]["properties"][0]["quality"] == "error");
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> tiny;
    assert(!web::writeCapabilities(tiny.to<JsonObject>(), registry, 30).ok());
    document.clear();
    assert(config::writeSettingsSchema(document.to<JsonObject>()).ok());
    assert(document["fields"].size() == 33);
    assert(document["fields"][9]["maximum"] == 5);
    assert(registry.add({&fan, &fan, &fan, &fan}).ok());
    peripherals::BleTargets targets;
    for (size_t i = 0; i < 8; ++i)
        targets.addresses[targets.count++] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, static_cast<uint8_t>(i + 1)};
    assert(presence.setTargets(targets, 40).ok());
    assert(registry.add({&presence, &presence, &presence, nullptr}).ok());
    assert(registry.add({&transport, &transport, &transport, &transport}).ok());
    peripherals::CurrentInputs current{registry};
    assert(registry.add({&current, &current, &current, nullptr}).ok());
    FakeRuleUdp udpHardware;
    peripherals::UdpRules udp{udpHardware};
    assert(registry.add({&udp, &udp, &udp, nullptr}).ok());
    assert(registry.begin(50).ok());
    document.clear();
    assert(web::writeCapabilities(document.to<JsonObject>(), registry, 50).ok());
    assert(document["modules"].size() == 8 && !document.overflowed());
    assert(document["modules"][4]["properties"].size() == 9);
    assert(document["modules"][4]["properties"][8]["id"] == "ble.presence/AA:BB:CC:DD:EE:08/in_range");
    std::string json;
    serializeJson(document, json);
    assert(json.size() < web::capabilitiesJsonCapacity);
    document.clear();
    assert(!deserializeJson(document, R"({"id":"transport.main/serial/send","parameters":{"port":2,"data":"hello\u0000hidden"}})"));
    assert(!web::executeJsonAction(registry, document.as<JsonVariantConst>(), false).result.ok());
    document.clear();
    assert(!deserializeJson(document, R"({"id":"io.main/relay/set\u0000ignored","parameters":{"channel":1,"on":true}})"));
    assert(!web::executeJsonAction(registry, document.as<JsonVariantConst>(), false).result.ok());
}
void testBleScanJson() {
    peripherals::BleScanSnapshot scan;
    scan.count = scan.capacity;
    for (size_t index = 0; index < scan.count; ++index) {
        scan.devices[index].name.fill('x'); scan.devices[index].name.back() = 0;
        scan.devices[index].address.fill(index);
        scan.devices[index].rssi = -99;
    }
    DynamicJsonDocument doc(web::bleScanJsonCapacity);
    assert(web::writeBleScan(doc.to<JsonObject>(), scan).ok() && !doc.overflowed());
    std::string json; serializeJson(doc, json);
    doc.clear();
    assert(!deserializeJson(doc, json));
    assert(doc["devices"].size() == 24 && doc["devices"][23]["name"].as<std::string>().size() == 31);
    assert(doc["devices"][0]["address"] == "00:00:00:00:00:00");
    assert(doc["devices"][23]["address"] == "17:17:17:17:17:17");
    StaticJsonDocument<32> tiny;
    assert(!web::writeBleScan(tiny.to<JsonObject>(), scan).ok());
}
void testLayouts() {
    FakeIo hardware;
    peripherals::BoardIo io{hardware};
    core::PeripheralRegistry registry;
    assert(registry.add({&io, &io, &io, &io}).ok());
    DynamicJsonDocument document(display::layoutJsonCapacity);
    const char *json = R"({"schemaVersion":1,"pages":[{"rows":2,"columns":3},{"rows":1,"columns":1}],"widgets":[{"page":0,"row":1,"column":2,"label":"DI1","property":"io.main/input/1"},{"page":1,"row":0,"column":0,"label":"R1","property":"io.main/relay/1"}]})";
    assert(!deserializeJson(document, json));
    display::ScreenLayout layout;
    assert(display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    assert(layout.pageCount == 2 && layout.widgetCount == 2);
    const auto cell = display::widgetCell(layout.pages[0], layout.widgets[0], 128, 64);
    assert(cell.x == 85 && cell.y == 32 && cell.width == 43 && cell.height == 32);
    document.clear();
    assert(display::writeLayout(document.to<JsonObject>(), layout).ok());
    display::ScreenLayout roundtrip;
    assert(display::readLayout(document.as<JsonVariantConst>(), registry, roundtrip).ok());
    assert(std::strcmp(roundtrip.widgets[0].property.data(), layout.widgets[0].property.data()) == 0);
    document["widgets"][0]["font"] = 2;
    document["widgets"][0]["textSize"] = 3;
    document["widgets"][0]["unit"] = "rpm";
    assert(display::readLayout(document.as<JsonVariantConst>(), registry, roundtrip).ok());
    assert(roundtrip.widgets[0].font == 2 && roundtrip.widgets[0].textSize == 3 &&
           std::strcmp(roundtrip.widgets[0].unit.data(), "rpm") == 0);
    document["widgets"][0]["font"] = 3;
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, roundtrip).ok());
    document["widgets"][0]["font"] = 0;
    document["widgets"][0]["textSize"] = 0;
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, roundtrip).ok());
    document["widgets"][0]["textSize"] = 1;
    document["widgets"][0]["unit"] = "012345678";
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, roundtrip).ok());
    document["widgets"][0]["unit"] = "";
    const auto before = layout;
    document["widgets"][0]["property"] = "unregistered";
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    assert(layout.widgetCount == before.widgetCount &&
           std::strcmp(layout.widgets[0].property.data(), before.widgets[0].property.data()) == 0);
    document["widgets"][0]["property"] = "io.main/input/1";
    document["pages"][0]["rows"] = 5;
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    document["pages"][0]["rows"] = 2;
    document["widgets"][0]["row"] = 256;
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    document["widgets"][0]["row"] = 1;
    document["widgets"][1]["page"] = 0;
    document["widgets"][1]["row"] = 1;
    document["widgets"][1]["column"] = 2;
    assert(display::readLayout(document.as<JsonVariantConst>(), registry, layout).code ==
           core::ErrorCode::ResourceConflict);
    document.clear();
    document["schemaVersion"] = 1;
    auto pages = document.createNestedArray("pages");
    for (size_t index = 0; index < display::maxPages; ++index) {
        auto page = pages.createNestedObject(); page["rows"] = 1; page["columns"] = 2;
    }
    auto widgets = document.createNestedArray("widgets");
    for (size_t index = 0; index < display::maxWidgets; ++index) {
        auto widget = widgets.createNestedObject();
        widget["page"] = index / 2; widget["row"] = 0; widget["column"] = index % 2;
        widget["property"] = "io.main/input/1"; widget["label"] = "DI";
    }
    assert(display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    assert(layout.pageCount == 8 && layout.widgetCount == 16);
    widgets.createNestedObject();
    assert(!display::readLayout(document.as<JsonVariantConst>(), registry, layout).ok());
    assert(layout.pageCount == 8 && layout.widgetCount == 16);
}
}
void runCapabilitiesTests() {
    testFanAndConditions();
    testIoAndActions(); testCapabilityJson(); testLayouts(); testBleScanJson();
    std::cout << "PASS: 5 end-to-end groups (PWM/IFTTT, IO, capabilities/schema, screen, BLE JSON)\n";
}
