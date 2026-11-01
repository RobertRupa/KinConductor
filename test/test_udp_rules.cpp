#include "peripherals/udp_rules.h"
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
using namespace kc868a6;
namespace {
class FakeUdp final : public peripherals::IUdpRulesHardware {
public:
    struct Packet { std::string text; std::array<uint8_t, 4> ip{192,168,1,2}; uint16_t port = 9000; };
    std::array<std::deque<Packet>, 4> packets;
    std::array<uint16_t, 4> ports{};
    bool online = true, failOpen = false;
    bool connected() const override { return online; }
    core::Result open(uint8_t socket, uint16_t port) override {
        if (failOpen) return {core::ErrorCode::OperationFailed, "bind"};
        ports[socket] = port; return {};
    }
    void close(uint8_t socket) override { ports[socket] = 0; packets[socket].clear(); }
    core::Result receive(uint8_t socket, char *data, size_t capacity, size_t &size,
                        std::array<uint8_t, 4> &ip, uint16_t &port, bool &received) override {
        size = 0; received = !packets[socket].empty();
        if (!received) return {};
        const auto packet = packets[socket].front(); packets[socket].pop_front();
        if (packet.text.size() > capacity) return {core::ErrorCode::CapacityExceeded, "payload"};
        size = packet.text.size(); std::memcpy(data, packet.text.data(), size);
        ip = packet.ip; port = packet.port; return {};
    }
};
void testConfig() {
    DynamicJsonDocument json(4096);
    assert(!deserializeJson(json, R"([{"enabled":true,"source":"udpPacket","udp":{"port":5000,"remoteIp":"192.168.1.2","remotePort":9000,"match":"exact","text":"hello"}}])"));
    peripherals::UdpRulesConfig config;
    assert(peripherals::readUdpRules(json.as<JsonVariantConst>(), 0, config).ok());
    assert(config[0].enabled && config[0].port == 5000 && config[0].textSize == 5);
    assert(!peripherals::readUdpRules(json.as<JsonVariantConst>(), 5000, config).ok());
    assert(peripherals::matchesUdpRule(config[0], {192,168,1,2}, 9000, "hello"));
    assert(!peripherals::matchesUdpRule(config[0], {192,168,1,3}, 9000, "hello"));
    assert(!peripherals::matchesUdpRule(config[0], {192,168,1,2}, 9001, "hello"));
    assert(!peripherals::matchesUdpRule(config[0], {192,168,1,2}, 9000, "helloworld"));
    json.clear();
    JsonArray manyRules = json.to<JsonArray>();
    for (size_t index = 0; index < peripherals::iftttRuleCapacity; ++index) {
        JsonObject rule = manyRules.createNestedObject();
        rule["enabled"] = true;
        rule["source"] = "udpPacket";
        JsonObject udp = rule.createNestedObject("udp");
        udp["port"] = 5000;
        udp["remoteIp"] = "";
        udp["remotePort"] = 0;
        udp["match"] = "exact";
        udp["text"] = "";
    }
    assert(peripherals::readUdpRules(json.as<JsonVariantConst>(), 0, config).ok());
    assert(config[peripherals::iftttRuleCapacity - 1].enabled);
    manyRules[4]["udp"]["port"] = 5004;
    manyRules[5]["udp"]["port"] = 5005;
    manyRules[6]["udp"]["port"] = 5006;
    manyRules[7]["udp"]["port"] = 5007;
    assert(!peripherals::readUdpRules(json.as<JsonVariantConst>(), 0, config).ok());
    manyRules[4]["udp"]["port"] = 5000;
    manyRules[5]["udp"]["port"] = 5000;
    manyRules.createNestedObject();
    assert(!peripherals::readUdpRules(json.as<JsonVariantConst>(), 0, config).ok());
    const char *invalid[] = {
        R"({"port":0,"remoteIp":"","remotePort":0,"match":"exact","text":""})",
        R"({"port":65536,"remoteIp":"","remotePort":0,"match":"exact","text":""})",
        R"({"port":5353,"remoteIp":"","remotePort":0,"match":"exact","text":""})",
        R"({"port":5000,"remoteIp":"256.1.1.1","remotePort":0,"match":"exact","text":""})",
        R"({"port":5000,"remoteIp":"1.1.1","remotePort":0,"match":"exact","text":""})",
        R"({"port":5000,"remoteIp":"a.local","remotePort":0,"match":"exact","text":""})",
        R"({"port":5000,"remoteIp":"","remotePort":-1,"match":"exact","text":""})",
        R"({"port":5000,"remoteIp":"","remotePort":0,"match":"other","text":""})",
        R"({"port":5000,"remoteIp":"","remotePort":0,"match":"exact","text":"hello\u0000hidden"})"
    };
    for (const auto *text : invalid) {
        json.clear(); assert(!deserializeJson(json, text));
        auto candidate = config[0];
        assert(!peripherals::readUdpRule(json.as<JsonVariantConst>(), candidate).ok());
        assert(candidate.port == config[0].port && candidate.textSize == config[0].textSize);
    }
}
void testRuntime() {
    peripherals::UdpRulesConfig config;
    config[0].enabled = config[1].enabled = config[2].enabled = true;
    config[0].port = config[1].port = 5000; config[2].port = 5001;
    config[7].enabled = true; config[7].port = 5000;
    config[1].contains = true; config[1].text[0] = 'x'; config[1].textSize = 1;
    FakeUdp hardware;
    peripherals::UdpRules rules{hardware}; rules.configure(config);
    assert(rules.begin(10).ok());
    assert(hardware.ports[0] == 5000 && hardware.ports[1] == 5001 && hardware.ports[2] == 0);
    hardware.packets[0].push_back({"hello"}); hardware.packets[0].push_back({"xyz"});
    hardware.packets[1].push_back({"other"});
    assert(rules.tick(20).ok());
    assert(rules.consume(0) == 2 && rules.consume(1) == 1 && rules.consume(2) == 1);
    assert(rules.consume(7) == 2);
    assert(rules.consume(0) == 0);
    core::PropertySnapshot value;
    assert(rules.readProperty("udp.rules/1/matches", value).ok());
    assert(std::get<uint32_t>(value.value) == 2 && value.updatedAtMs == 20 && value.quality == core::Quality::Valid);
    assert(rules.readProperty("udp.rules/4/matches", value).ok() && value.quality == core::Quality::Disabled);
        assert(rules.readProperty("udp.rules/8/matches", value).ok() &&
            std::get<uint32_t>(value.value) == 2 && value.quality == core::Quality::Valid);
    hardware.online = false; assert(rules.tick(30).ok());
    assert(hardware.ports[0] == 0 && rules.consume(0) == 0);
    hardware.online = true; hardware.failOpen = true; assert(!rules.tick(40).ok());
    hardware.failOpen = false; assert(rules.tick(50).ok());
    hardware.packets[0].push_back({std::string(257, 'a')});
    assert(!rules.tick(60).ok() && rules.consume(0) == 0);
    rules.configure({}); assert(rules.tick(70).ok() && hardware.ports[0] == 0);
}
}
void runUdpRuleTests() { testConfig(); testRuntime(); std::cout << "PASS: 2 UDP rule groups (ports, filters, queues, reconnect, errors)\n"; }
