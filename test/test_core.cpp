#include "core/peripheral_registry.h"
#include "peripherals/analog_inputs.h"
#include "web/analog_status.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

using namespace kc868a6;

namespace {

class FakePeripheral final : public core::IPeripheral,
                             public core::IPropertyProvider,
                             public core::ITriggerProvider,
                             public core::IActionProvider {
public:
    core::PeripheralDescriptor identity{"fake.main", "fake"};
    std::array<core::PropertyDescriptor, 1> propertyDescriptors{{
        {"fake.main/property/value", core::ValueType::Number, "degC", 100},
    }};
    std::array<core::TriggerDescriptor, 2> triggerDescriptors{{
        {"fake.main/trigger/condition", core::TriggerKind::Condition,
         propertyDescriptors[0].id},
        {"fake.main/trigger/event", core::TriggerKind::Event, {}},
    }};
    std::array<core::ParameterDescriptor, 2> parameterDescriptors{{
        {"on", core::ValueType::Boolean, true},
        {"duration", core::ValueType::UnsignedInteger, false},
    }};
    std::array<core::ActionDescriptor, 1> actionDescriptors{{
        {"fake.main/action/set", parameterDescriptors},
    }};
    core::PropertySnapshot snapshot{25.0, core::Quality::Valid, 0};
    core::Result beginResult;
    core::Result tickResult;
    core::Result readResult;
    core::ActionResult actionResult{{}, true, false};
    size_t beginCalls = 0;
    size_t tickCalls = 0;
    size_t stopCalls = 0;
    size_t actionCalls = 0;
    bool ready = false;

    const core::PeripheralDescriptor &descriptor() const override { return identity; }
    core::Result begin(uint32_t) override {
        ++beginCalls;
        ready = beginResult.ok();
        return beginResult;
    }
    core::Result tick(uint32_t) override {
        ++tickCalls;
        return tickResult;
    }
    void stop() override {
        ++stopCalls;
        ready = false;
    }
    core::Health health() const override { return {ready, beginResult.code}; }
    core::List<core::PropertyDescriptor> properties() const override {
        return propertyDescriptors;
    }
    core::List<core::TriggerDescriptor> triggers() const override {
        return triggerDescriptors;
    }
    core::List<core::ActionDescriptor> actions() const override {
        return actionDescriptors;
    }
    core::Result readProperty(std::string_view id,
                              core::PropertySnapshot &output) const override {
        if (id != propertyDescriptors[0].id) return {core::ErrorCode::NotFound, id};
        output = snapshot;
        return readResult;
    }
    core::ActionResult execute(std::string_view,
                               core::List<core::Parameter>) override {
        ++actionCalls;
        return actionResult;
    }

    core::PeripheralBinding binding() { return {this, this, this, this}; }
};

class FakeAnalogHardware final : public peripherals::IAnalogInputHardware {
public:
    static constexpr std::array<uint8_t, 4> pins{{36, 39, 34, 35}};
    std::array<uint16_t, 4> values{{0, 255, 256, 4095}};
    std::array<size_t, 4> reads{};
    size_t beginCalls = 0;
    int failedChannel = -1;
    core::Result beginResult;

    core::Result begin() override {
        ++beginCalls;
        return beginResult;
    }
    core::Result read(uint8_t pin, uint16_t &value) override {
        for (size_t index = 0; index < pins.size(); ++index) {
            if (pins[index] != pin) continue;
            ++reads[index];
            if (static_cast<int>(index) == failedChannel) {
                return {core::ErrorCode::OperationFailed, "adc_read"};
            }
            value = values[index];
            return {};
        }
        return {core::ErrorCode::InvalidArgument, "adc_pin"};
    }
};

void testValueTypes() {
    assert(core::valueMatchesType(true, core::ValueType::Boolean));
    assert(!core::valueMatchesType(uint32_t{1}, core::ValueType::Boolean));
    assert(core::valueMatchesType(int32_t{-2}, core::ValueType::SignedInteger));
    assert(core::valueMatchesType(uint32_t{4095}, core::ValueType::UnsignedInteger));
    assert(core::valueMatchesType(25.0, core::ValueType::Number));
    assert(!core::valueMatchesType(
        std::numeric_limits<double>::quiet_NaN(), core::ValueType::Number));
    assert(!core::valueMatchesType(
        std::numeric_limits<double>::infinity(), core::ValueType::Number));
    assert(core::valueMatchesType(std::string_view{"hello"}, core::ValueType::Text));
}

void testRegistrationAndLifecycle() {
    FakePeripheral peripheral;
    core::PeripheralRegistry registry;
    assert(registry.add({}).code == core::ErrorCode::InvalidArgument);
    assert(registry.add(peripheral.binding()).ok());
    assert(registry.size() == 1 && registry.revision() == 1);
    assert(registry.findProperty(peripheral.propertyDescriptors[0].id));
    assert(registry.findTrigger(peripheral.triggerDescriptors[0].id));
    assert(registry.findTrigger(peripheral.triggerDescriptors[1].id));
    assert(registry.findAction(peripheral.actionDescriptors[0].id));
    assert(registry.add(peripheral.binding()).code == core::ErrorCode::DuplicateId);
    assert(registry.revision() == 1);
    assert(registry.tick(10).ok() && peripheral.tickCalls == 0);
    assert(registry.begin(10).ok());
    assert(registry.begin(11).ok() && peripheral.beginCalls == 1);
    assert(registry.tick(12).ok() && peripheral.tickCalls == 1);
    registry.stop();
    assert(peripheral.stopCalls == 1 && !peripheral.ready);
    registry.stop();
    assert(peripheral.stopCalls == 1);
    assert(registry.begin(13).ok() && peripheral.beginCalls == 2);
    assert(registry.remove(peripheral.identity.id).ok());
    assert(peripheral.stopCalls == 2 && registry.revision() == 2);
    assert(registry.findProperty(peripheral.propertyDescriptors[0].id) == nullptr);
    assert(registry.findTrigger(peripheral.triggerDescriptors[0].id) == nullptr);
    assert(registry.findAction(peripheral.actionDescriptors[0].id) == nullptr);
    assert(registry.remove("missing").code == core::ErrorCode::NotFound);
}

void testOptionalProvidersAndCapacity() {
    std::array<FakePeripheral, core::PeripheralRegistry::capacity + 1> peripherals;
    const std::array<std::string_view, 9> ids{{
        "fake.0", "fake.1", "fake.2", "fake.3", "fake.4",
        "fake.5", "fake.6", "fake.7", "fake.8",
    }};
    core::PeripheralRegistry registry;
    for (size_t index = 0; index < core::PeripheralRegistry::capacity; ++index) {
        peripherals[index].identity.id = ids[index];
        assert(registry.add({&peripherals[index], nullptr, nullptr, nullptr}).ok());
    }
    peripherals.back().identity.id = ids.back();
    assert(registry.add({&peripherals.back(), nullptr, nullptr, nullptr}).code ==
           core::ErrorCode::CapacityExceeded);
    assert(registry.revision() == core::PeripheralRegistry::capacity);
    assert(registry.findAction("missing") == nullptr);
    assert(registry.begin(0).ok());
    assert(registry.tick(1).ok());
    for (size_t index = 0; index < core::PeripheralRegistry::capacity; ++index) {
        assert(peripherals[index].beginCalls == 1 && peripherals[index].tickCalls == 1);
    }
}

void testAtomicDescriptorValidation() {
    FakePeripheral first;
    FakePeripheral second;
    second.identity.id = "fake.second";
    core::PeripheralRegistry registry;
    assert(registry.add(first.binding()).ok());
    assert(registry.add(second.binding()).code == core::ErrorCode::DuplicateId);
    assert(registry.size() == 1 && registry.revision() == 1);

    core::PeripheralRegistry invalidRegistry;
    second.triggerDescriptors[0].propertyId = "missing";
    assert(invalidRegistry.add(second.binding()).code == core::ErrorCode::InvalidArgument);
    assert(invalidRegistry.size() == 0 && invalidRegistry.revision() == 0);
    second.triggerDescriptors[0].propertyId = second.propertyDescriptors[0].id;
    second.parameterDescriptors[1].id = second.parameterDescriptors[0].id;
    assert(invalidRegistry.add(second.binding()).code == core::ErrorCode::DuplicateId);
    second.parameterDescriptors[1].id = "duration";
    second.triggerDescriptors[1].id = second.triggerDescriptors[0].id;
    assert(invalidRegistry.add(second.binding()).code == core::ErrorCode::DuplicateId);
    second.triggerDescriptors[1].id = "fake.second/trigger/event";
    second.actionDescriptors[0].id = second.propertyDescriptors[0].id;
    assert(invalidRegistry.add(second.binding()).code == core::ErrorCode::DuplicateId);
    second.actionDescriptors[0].id = "fake.second/action/set";
    second.propertyDescriptors[0].id = first.actionDescriptors[0].id;
    second.triggerDescriptors[0].propertyId = second.propertyDescriptors[0].id;
    assert(registry.add(second.binding()).code == core::ErrorCode::DuplicateId);
    assert(registry.size() == 1 && registry.revision() == 1);
}

void testSnapshotQualityAndRollover() {
    FakePeripheral peripheral;
    core::PeripheralRegistry registry;
    assert(registry.add(peripheral.binding()).ok());
    core::PropertySnapshot snapshot{uint32_t{4095}, core::Quality::Valid, 99};
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 0, snapshot).code ==
           core::ErrorCode::Unavailable);
    assert(snapshot.quality == core::Quality::Unavailable);
    assert(registry.begin(0).ok());
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 100, snapshot).ok());
    assert(snapshot.quality == core::Quality::Valid);
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 101, snapshot).ok());
    assert(snapshot.quality == core::Quality::Stale);
    peripheral.snapshot.updatedAtMs = UINT32_MAX - 50;
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 20, snapshot).ok());
    assert(snapshot.quality == core::Quality::Valid);
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 51, snapshot).ok());
    assert(snapshot.quality == core::Quality::Stale);

    peripheral.snapshot.value = uint32_t{1};
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 52, snapshot).code ==
           core::ErrorCode::InvalidArgument);
    assert(snapshot.quality == core::Quality::Error);
    peripheral.snapshot.value = std::numeric_limits<double>::quiet_NaN();
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 52, snapshot).code ==
           core::ErrorCode::InvalidArgument);
    peripheral.snapshot.quality = core::Quality::Unavailable;
    assert(registry.readProperty(peripheral.propertyDescriptors[0].id, 52, snapshot).ok());
    assert(snapshot.quality == core::Quality::Unavailable);

    peripheral.readResult = {core::ErrorCode::OperationFailed, "read"};
    assert(!registry.readProperty(peripheral.propertyDescriptors[0].id, 52, snapshot).ok());
    assert(snapshot.quality == core::Quality::Error);
    assert(snapshot.error == core::ErrorCode::OperationFailed);
    assert(registry.readProperty("missing", 52, snapshot).code == core::ErrorCode::NotFound);
    assert(snapshot.quality != core::Quality::Valid);
}

void testActionValidationAndResults() {
    FakePeripheral peripheral;
    core::PeripheralRegistry registry;
    assert(registry.add(peripheral.binding()).ok());
    const std::array<core::Parameter, 1> valid{{{"on", true}}};
    const auto id = peripheral.actionDescriptors[0].id;
    assert(registry.execute(id, valid).result.code == core::ErrorCode::Unavailable);
    assert(registry.begin(0).ok());
    assert(registry.execute(id, valid).applied);
    assert(!registry.execute(id, valid).confirmed);
    assert(peripheral.actionCalls == 2);
    assert(registry.execute(id, {}).result.code == core::ErrorCode::InvalidArgument);
    assert(registry.execute(id, {nullptr, 1}).result.code == core::ErrorCode::InvalidArgument);
    const std::array<core::Parameter, 1> wrongType{{{"on", uint32_t{1}}}};
    assert(registry.execute(id, wrongType).result.code == core::ErrorCode::InvalidArgument);
    const std::array<core::Parameter, 1> unknown{{{"unknown", true}}};
    assert(registry.execute(id, unknown).result.code == core::ErrorCode::InvalidArgument);
    const std::array<core::Parameter, 2> duplicate{{{"on", true}, {"on", false}}};
    assert(registry.execute(id, duplicate).result.code == core::ErrorCode::DuplicateId);
    assert(peripheral.actionCalls == 2);
    assert(registry.execute("missing", valid).result.code == core::ErrorCode::NotFound);
    peripheral.actionResult = {{core::ErrorCode::OperationFailed, id}, true, false};
    const auto failed = registry.execute(id, valid);
    assert(!failed.result.ok() && failed.applied && !failed.confirmed);
    peripheral.ready = false;
    assert(registry.execute(id, valid).result.code == core::ErrorCode::Unavailable);
}

void testBeginAndTickFailures() {
    FakePeripheral failed;
    FakePeripheral healthy;
    healthy.identity.id = "healthy";
    failed.beginResult = {core::ErrorCode::OperationFailed, "failed"};
    core::PeripheralRegistry registry;
    assert(registry.add({&failed, nullptr, nullptr, nullptr}).ok());
    assert(registry.add({&healthy, nullptr, nullptr, nullptr}).ok());
    assert(!registry.begin(0).ok());
    assert(healthy.ready && !failed.ready);
    assert(registry.tick(1).ok());
    assert(failed.tickCalls == 0 && healthy.tickCalls == 1);
    failed.beginResult = {};
    assert(registry.begin(2).ok() && failed.ready);
    assert(failed.beginCalls == 2 && healthy.beginCalls == 1);
    failed.tickResult = {core::ErrorCode::OperationFailed, "tick"};
    assert(!registry.tick(3).ok());
    assert(healthy.tickCalls == 2);
}

void testAnalogChannelsAndLegacyJson() {
    FakeAnalogHardware hardware;
    peripherals::AnalogInputs analog{hardware, FakeAnalogHardware::pins};
    core::PeripheralRegistry registry;
    assert(registry.add({&analog, &analog, &analog, nullptr}).ok());
    assert(analog.properties().size == 4 && analog.triggers().size == 4);
    assert(analog.descriptor().id == "adc.main");
    assert(registry.begin(0).ok() && hardware.beginCalls == 1);
    core::PropertySnapshot snapshot;
    const auto properties = analog.properties();
    assert(registry.readProperty(properties.data[0].id, 0, snapshot).ok());
    assert(snapshot.quality == core::Quality::Unavailable);
    assert(registry.tick(50).ok());
    for (size_t channel = 0; channel < 4; ++channel) {
        assert(hardware.reads[channel] == 1);
        assert(analog.rawValues()[channel] == hardware.values[channel]);
        assert(registry.readProperty(properties.data[channel].id, 50, snapshot).ok());
        assert(snapshot.quality == core::Quality::Valid && snapshot.updatedAtMs == 50);
        const auto *value = std::get_if<uint32_t>(&snapshot.value);
        assert(value && *value == hardware.values[channel]);
    }

    StaticJsonDocument<256> document;
    assert(web::appendAnalogStatus(document["analog"].to<JsonArray>(), analog).ok());
    std::string json;
    serializeJson(document, json);
    assert(!document.overflowed());
    assert(json == "{\"analog\":[0,255,256,4095]}");
    hardware.values = {{2048, 4095, 0, 256}};
    assert(registry.tick(60).ok());
    document.clear();
    assert(web::appendAnalogStatus(document["analog"].to<JsonArray>(), analog).ok());
    json.clear();
    serializeJson(document, json);
    assert(json == "{\"analog\":[2048,4095,0,256]}");
    StaticJsonDocument<JSON_ARRAY_SIZE(1)> tooSmall;
    assert(web::appendAnalogStatus(tooSmall.to<JsonArray>(), analog).code ==
           core::ErrorCode::CapacityExceeded);
    assert(tooSmall.overflowed());
}

void testAnalogErrorRecoveryAndStop() {
    FakeAnalogHardware hardware;
    peripherals::AnalogInputs analog{hardware, FakeAnalogHardware::pins};
    assert(analog.tick(0).code == core::ErrorCode::Unavailable);
    hardware.beginResult = {core::ErrorCode::OperationFailed, "adc_init"};
    assert(!analog.begin(0).ok() && !analog.health().ready);
    hardware.beginResult = {};
    assert(analog.begin(1).ok());
    assert(analog.tick(10).ok());
    hardware.failedChannel = 2;
    assert(!analog.tick(20).ok() && !analog.health().ready);
    core::PropertySnapshot snapshot;
    const auto properties = analog.properties();
    assert(analog.readProperty(properties.data[2].id, snapshot).ok());
    assert(snapshot.quality == core::Quality::Error && snapshot.updatedAtMs == 10);
    assert(analog.rawValues()[2] == 256);
    assert(hardware.reads[3] == 2);
    hardware.failedChannel = -1;
    hardware.values[2] = 4096;
    assert(analog.tick(30).code == core::ErrorCode::InvalidArgument);
    hardware.values[2] = 4095;
    assert(analog.tick(40).ok() && analog.health().ready);
    assert(analog.readProperty(properties.data[2].id, snapshot).ok());
    assert(snapshot.quality == core::Quality::Valid && snapshot.updatedAtMs == 40);
    analog.stop();
    assert(!analog.health().ready);
    assert(analog.readProperty(properties.data[2].id, snapshot).ok());
    assert(snapshot.quality == core::Quality::Unavailable);
    assert(analog.begin(50).ok() && analog.rawValues()[2] == 0);
    assert(analog.readProperty("missing", snapshot).code == core::ErrorCode::NotFound);
}

}  // namespace

void runConfigTests();
void runBoardTests();
void runCapabilitiesTests();
void runBlePresenceTests();
void runTransportTests();
void runMeasurementTests();
void runUdpRuleTests();

int main() {
    testValueTypes();
    testRegistrationAndLifecycle();
    testOptionalProvidersAndCapacity();
    testAtomicDescriptorValidation();
    testSnapshotQualityAndRollover();
    testActionValidationAndResults();
    testBeginAndTickFailures();
    testAnalogChannelsAndLegacyJson();
    testAnalogErrorRecoveryAndStop();
    std::cout << "PASS: 9 native test groups (core contracts, registry, ADC, legacy JSON)\n";
    runConfigTests();
    runBoardTests();
    runCapabilitiesTests();
    runBlePresenceTests();
    runTransportTests();
    runMeasurementTests();
    runUdpRuleTests();
}
