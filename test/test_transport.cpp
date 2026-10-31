#include "peripherals/transport.h"
#include "core/peripheral_registry.h"

#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>

using namespace kc868a6;
namespace {
class FakeTransport final : public peripherals::ITransportHardware {
public:
    bool connected = true;
    bool failListen = false;
    bool failRead = false;
    core::ActionResult sendResult{{}, true, false};
    unsigned listens = 0;
    unsigned stops = 0;
    unsigned sends = 0;
    std::array<unsigned, 2> reads{};
    uint16_t listenPort = 0;
    std::deque<std::string> packets;
    std::array<std::deque<unsigned char>, 2> serial;
    bool wifiConnected() const override { return connected; }
    core::Result listenUdp(uint16_t port) override {
        ++listens;
        if (failListen) return {core::ErrorCode::OperationFailed, "udp.listen"};
        listenPort = port;
        return {};
    }
    void stopUdp() override { ++stops; listenPort = 0; }
    core::Result receiveUdp(char *data, size_t capacity, size_t &size, bool &received) override {
        size = 0;
        received = !packets.empty();
        if (!received) return {};
        const std::string packet = packets.front();
        packets.pop_front();
        if (packet.size() > capacity) return {core::ErrorCode::CapacityExceeded, "udp.rx.overflow"};
        size = packet.size();
        if (size != 0) std::memcpy(data, packet.data(), size);
        return {};
    }
    core::ActionResult sendUdp(std::string_view, uint16_t, std::string_view) override {
        ++sends; return sendResult;
    }
    core::Result serialRead(uint8_t port, int &byte) override {
        ++reads[port - 1];
        byte = -1;
        if (failRead) return {core::ErrorCode::OperationFailed, "serial.rx"};
        auto &queue = serial[port - 1];
        if (!queue.empty()) { byte = queue.front(); queue.pop_front(); }
        return {};
    }
    core::ActionResult serialSend(uint8_t, std::string_view) override {
        ++sends; return sendResult;
    }
    void push(uint8_t port, std::string_view bytes) {
        for (unsigned char byte : bytes) serial[port - 1].push_back(byte);
    }
};

core::PropertySnapshot property(peripherals::Transport &transport, size_t index) {
    core::PropertySnapshot snapshot;
    assert(transport.readProperty(transport.properties().data[index].id, snapshot).ok());
    return snapshot;
}
uint32_t count(peripherals::Transport &transport, size_t receiver) {
    return std::get<uint32_t>(property(transport, 1 + receiver * 2).value);
}
std::string text(peripherals::Transport &transport, size_t receiver) {
    return std::string(std::get<std::string_view>(property(transport, 2 + receiver * 2).value));
}
core::ActionResult sendSerial(peripherals::Transport &transport, uint32_t port,
                              std::string_view data) {
    const std::array<core::Parameter, 2> args{{{"data", data}, {"port", port}}};
    return transport.execute("transport.main/serial/send", args);
}
core::ActionResult sendUdp(peripherals::Transport &transport, std::string_view host,
                           uint32_t port, std::string_view data) {
    const std::array<core::Parameter, 3> args{{
        {"host", host}, {"port", port}, {"data", data},
    }};
    return transport.execute("transport.main/udp/send", args);
}

void testLifecycleAndRegistry() {
    FakeTransport hardware;
    peripherals::Transport transport(hardware);
    assert(!transport.tick(0).ok());
    assert(!sendSerial(transport, 1, "x").result.ok());
    core::PeripheralRegistry registry;
    assert(registry.add({&transport, &transport, &transport, &transport}).ok());
    assert(registry.begin(0).ok());
    assert(registry.tick(1).ok());
    assert(hardware.listens == 0);
    assert(property(transport, 1).quality == core::Quality::Disabled);
    assert(std::get<bool>(property(transport, 0).value));
    for (const auto &trigger : std::array<core::TriggerDescriptor, 4>{{
             transport.triggers().data[0], transport.triggers().data[1],
             transport.triggers().data[2], transport.triggers().data[3]}}) {
        assert(registry.findTrigger(trigger.id));
        assert(registry.findProperty(trigger.propertyId));
    }
    peripherals::TransportConfig config;
    config.udpListenPort = 6000;
    assert(transport.configure(config, 2).ok());
    assert(hardware.listenPort == 6000);
    hardware.packets.emplace_back("hello");
    assert(registry.tick(3).ok());
    assert(count(transport, 0) == 1 && text(transport, 0) == "hello");
    hardware.connected = false;
    assert(registry.tick(4).ok());
    assert(!std::get<bool>(property(transport, 0).value));
    assert(property(transport, 1).quality == core::Quality::Unavailable);
    assert(hardware.listenPort == 0);
    const auto disconnected = sendUdp(transport, "example.org", 6000, "x");
    assert(disconnected.result.code == core::ErrorCode::Unavailable && !disconnected.applied);
    assert(hardware.sends == 0);
    hardware.connected = true;
    hardware.failListen = true;
    assert(!registry.tick(5).ok());
    assert(transport.health().ready);
    hardware.failListen = false;
    assert(registry.tick(6).ok());
    assert(hardware.listenPort == 6000);
    hardware.packets.emplace_back("recovered");
    assert(registry.tick(7).ok());
    assert(count(transport, 0) == 2 && property(transport, 1).quality == core::Quality::Valid);
    config.enabled = false;
    assert(transport.configure(config, 8).ok());
    const auto reads = hardware.reads;
    assert(registry.tick(9).ok() && reads == hardware.reads);
    assert(property(transport, 3).quality == core::Quality::Disabled);
    assert(!sendSerial(transport, 2, "x").result.ok());
    registry.stop();
    assert(property(transport, 0).quality == core::Quality::Unavailable);
    assert(registry.begin(10).ok() && count(transport, 0) == 0);
}

void testFramesAndErrors() {
    FakeTransport hardware;
    peripherals::Transport transport(hardware);
    peripherals::TransportConfig config;
    config.udpListenPort = 1000;
    assert(transport.configure(config, 0).ok());
    assert(transport.begin(0).ok());
    hardware.push(1, "hello\r\nworld\n");
    hardware.push(2, "idle");
    assert(transport.tick(1).ok());
    assert(count(transport, 1) == 2 && text(transport, 1) == "world");
    assert(count(transport, 2) == 0);
    assert(transport.tick(25).ok() && count(transport, 2) == 0);
    assert(transport.tick(26).ok() && text(transport, 2) == "idle");
    hardware.push(1, std::string(256, 'x') + "\r\n");
    assert(transport.tick(27).ok() && text(transport, 1).size() == 256);
    const auto previous = count(transport, 1);
    hardware.push(1, std::string(257, 'x'));
    assert(transport.tick(28).code == core::ErrorCode::CapacityExceeded);
    assert(count(transport, 1) == previous && property(transport, 3).quality == core::Quality::Error);
    hardware.push(1, "\n");
    assert(transport.tick(29).code == core::ErrorCode::CapacityExceeded);
    hardware.push(1, "fine\n");
    assert(transport.tick(30).ok() && text(transport, 1) == "fine");
    hardware.push(1, std::string(256, 'x') + "\rX\n");
    assert(transport.tick(31).code == core::ErrorCode::CapacityExceeded);
    hardware.push(1, std::string("\0\n", 2));
    assert(transport.tick(32).code == core::ErrorCode::InvalidArgument);
    hardware.push(1, "\xF0\x9F\x98\x80\n");
    assert(transport.tick(33).ok() && text(transport, 1) == "\xF0\x9F\x98\x80");
    hardware.packets.emplace_back(256, 'x');
    assert(transport.tick(34).ok() && text(transport, 0).size() == 256);
    hardware.packets.emplace_back(257, 'x');
    assert(transport.tick(35).code == core::ErrorCode::CapacityExceeded && count(transport, 0) == 1);
    hardware.packets.emplace_back("\xC0\xAF");
    assert(transport.tick(36).code == core::ErrorCode::InvalidArgument && count(transport, 0) == 1);
    hardware.packets.emplace_back("udp");
    assert(transport.tick(37).ok() && text(transport, 0) == "udp");
    hardware.packets.emplace_back("");
    assert(transport.tick(37).ok() && count(transport, 0) == 3 && text(transport, 0).empty());
    hardware.push(2, std::string(2000, 'z'));
    const auto before = hardware.reads[1];
    assert(transport.tick(38).code == core::ErrorCode::CapacityExceeded);
    assert(hardware.reads[1] - before == peripherals::Transport::serialReadBudget);
    hardware.serial[1].clear();
    assert(transport.tick(63).code == core::ErrorCode::CapacityExceeded);
    hardware.push(2, "wrap");
    assert(transport.tick(UINT32_MAX - 10).ok());
    assert(transport.tick(15).ok() && text(transport, 2) == "wrap");
    hardware.failRead = true;
    assert(transport.tick(16).code == core::ErrorCode::OperationFailed);
    hardware.failRead = false;
    hardware.push(1, "\xE2\x82");
    assert(transport.tick(17).ok());
    hardware.push(1, "\xAC\n");
    assert(transport.tick(18).ok() && text(transport, 1) == "\xE2\x82\xAC");
}

void testValidationAndOwnership() {
    FakeTransport hardware;
    peripherals::Transport transport(hardware);
    assert(transport.begin(0).ok());
    assert(sendUdp(transport, "example.org", 65535, "hello").result.ok());
    assert(sendSerial(transport, 2, "hello").applied);
    assert(!sendUdp(transport, "a", 0, "x").result.ok());
    assert(!sendUdp(transport, "a", 65536, "x").result.ok());
    assert(!sendUdp(transport, "bad host", 1, "x").result.ok());
    assert(!sendUdp(transport, "abc.", 1, "x").result.ok());
    assert(!sendUdp(transport, std::string(254, 'a'), 1, "x").result.ok());
    assert(!sendUdp(transport, "x", 1, std::string(257, 'x')).result.ok());
    assert(!sendSerial(transport, 0, "x").result.ok());
    assert(!sendSerial(transport, 3, "x").result.ok());
    assert(!sendSerial(transport, 1, std::string("a\0b", 3)).result.ok());
    const std::array<core::Parameter, 2> duplicates{{{"port", uint32_t{1}}, {"port", uint32_t{2}}}};
    assert(!transport.execute("transport.main/serial/send", duplicates).result.ok());
    const std::array<core::Parameter, 2> wrongType{{{"port", int32_t{1}}, {"data", std::string_view("x")}}};
    assert(!transport.execute("transport.main/serial/send", wrongType).result.ok());
    assert(transport.execute("unknown", {}).result.code == core::ErrorCode::NotFound);
    peripherals::TransportConfig config;
    config.serial1Reserved = true;
    assert(transport.configure(config, 1).ok());
    hardware.push(1, "reserved\n");
    const auto before = hardware.reads[0];
    assert(transport.tick(2).ok() && hardware.reads[0] == before);
    assert(property(transport, 3).quality == core::Quality::Unavailable);
    assert(sendSerial(transport, 1, "x").result.code == core::ErrorCode::ResourceConflict);
    assert(sendSerial(transport, 2, "x").result.ok());
    config.serialIdleMs = 0;
    assert(!transport.configure(config, 3).ok() && transport.config().serialIdleMs == 25);
    config.serialIdleMs = 60001;
    assert(!transport.configure(config, 4).ok());
    config.serialIdleMs = 25;
    config.serial2Enabled = false;
    assert(transport.configure(config, 5).ok());
    assert(sendSerial(transport, 2, "x").result.code == core::ErrorCode::Unavailable);
    const char *invalid[] = {"\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82", "\x7F"};
    for (const auto *bytes : invalid) assert(!peripherals::Transport::validateText(bytes).ok());
    assert(peripherals::Transport::validateText("\xE2\x82\xAC").ok());
    config.serial2Enabled = true;
    assert(transport.configure(config, 6).ok());
    hardware.sendResult = {{core::ErrorCode::OperationFailed, "tx"}, false, false};
    assert(sendSerial(transport, 2, "x").result.code == core::ErrorCode::OperationFailed);
}

void testRegistryValidationHook() {
    FakeTransport hardware;
    hardware.connected = false;
    peripherals::Transport transport(hardware);
    peripherals::TransportConfig config;
    config.enabled = false;
    config.serial1Reserved = true;
    assert(transport.configure(config, 0).ok());
    core::PeripheralRegistry registry;
    assert(registry.add({&transport, &transport, &transport, &transport}).ok());
    const std::string oversized(257, 'x');
    std::array<core::Parameter, 3> udp{{
        {"host", std::string_view("example.org")},
        {"port", uint32_t{1234}},
        {"data", std::string_view(oversized)},
    }};
    assert(registry.validateAction("transport.main/udp/send", udp).code ==
           core::ErrorCode::CapacityExceeded);
    udp[2].value = std::string_view("hello");
    udp[0].value = std::string_view("bad host");
    assert(registry.validateAction("transport.main/udp/send", udp).code ==
           core::ErrorCode::InvalidArgument);
    udp[0].value = std::string_view("example.org");
    udp[2].value = std::string_view("\xC0\xAF");
    assert(registry.validateAction("transport.main/udp/send", udp).code ==
           core::ErrorCode::InvalidArgument);
    udp[2].value = std::string_view("hello");
    assert(registry.validateAction("transport.main/udp/send", udp).ok());
    assert(transport.validate("transport.main/udp/send", udp).ok());
    std::array<core::Parameter, 2> serial{{
        {"port", uint32_t{1}}, {"data", std::string_view(oversized)},
    }};
    assert(registry.validateAction("transport.main/serial/send", serial).code ==
           core::ErrorCode::CapacityExceeded);
    serial[1].value = std::string_view("saved while reserved");
    assert(registry.validateAction("transport.main/serial/send", serial).ok());
    serial[0].value = uint32_t{3};
    assert(registry.validateAction("transport.main/serial/send", serial).code ==
           core::ErrorCode::InvalidArgument);
    assert(hardware.sends == 0 && hardware.listens == 0);
    assert(hardware.reads[0] == 0 && hardware.reads[1] == 0);
    assert(hardware.stops == 1);  // Only the explicit configure call above.
}
}

void runTransportTests() {
    testLifecycleAndRegistry();
    testFramesAndErrors();
    testValidationAndOwnership();
    testRegistryValidationHook();
    std::cout << "Transport tests passed\n";
}

#ifndef KC868_TRANSPORT_NO_MAIN
int main() {
    runTransportTests();
}
#endif
