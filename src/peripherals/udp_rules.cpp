#include "peripherals/udp_rules.h"
#include "peripherals/transport.h"
#include <cstring>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"udp.rules", "ifttt-udp-receiver"};
const std::array<core::PropertyDescriptor, 4> descriptors{{
    {"udp.rules/1/matches", core::ValueType::UnsignedInteger, ""},
    {"udp.rules/2/matches", core::ValueType::UnsignedInteger, ""},
    {"udp.rules/3/matches", core::ValueType::UnsignedInteger, ""},
    {"udp.rules/4/matches", core::ValueType::UnsignedInteger, ""},
}};
const std::array<core::TriggerDescriptor, 4> conditions{{
    {"udp.rules/1/matches/condition", core::TriggerKind::Condition, descriptors[0].id},
    {"udp.rules/2/matches/condition", core::TriggerKind::Condition, descriptors[1].id},
    {"udp.rules/3/matches/condition", core::TriggerKind::Condition, descriptors[2].id},
    {"udp.rules/4/matches/condition", core::TriggerKind::Condition, descriptors[3].id},
}};
}
core::Result readUdpRule(JsonVariantConst source, UdpRuleConfig &destination) {
    const core::Result invalid{core::ErrorCode::InvalidArgument, "udp_trigger"};
    if (!source.is<JsonObjectConst>() || source.size() != 5 || !source["port"].is<uint16_t>() ||
        source["port"].as<uint16_t>() == 0 || !source["remotePort"].is<uint16_t>() ||
        !source["remoteIp"].is<const char *>() || !source["text"].is<const char *>() ||
        !source["match"].is<const char *>()) return invalid;
    UdpRuleConfig next;
    next.port = source["port"].as<uint16_t>();
    if (next.port == 5353) return {core::ErrorCode::ResourceConflict, "udp_mdns_port"};
    next.remotePort = source["remotePort"].as<uint16_t>();
    const JsonString address = source["remoteIp"].as<JsonString>();
    const std::string_view ip{address.c_str(), address.size()};
    if (!ip.empty()) {
        size_t start = 0;
        for (size_t index = 0; index < 4; ++index) {
            const size_t end = ip.find('.', start);
            const size_t count = (end == std::string_view::npos ? ip.size() : end) - start;
            if (count == 0 || count > 3 || (index < 3) != (end != std::string_view::npos)) return invalid;
            unsigned number = 0;
            for (size_t i = start; i < start + count; ++i) {
                if (ip[i] < '0' || ip[i] > '9') return invalid;
                number = number * 10 + ip[i] - '0';
            }
            if (number > 255) return invalid;
            next.remoteIp[index] = number;
            start += count + 1;
        }
        next.filterIp = true;
    }
    const JsonString mode = source["match"].as<JsonString>();
    const std::string_view match{mode.c_str(), mode.size()};
    if (match != "exact" && match != "contains") return invalid;
    next.contains = match == "contains";
    const JsonString text = source["text"].as<JsonString>();
    const auto valid = Transport::validateText({text.c_str(), text.size()});
    if (!valid.ok() && text.size() != 0) return valid;
    if (text.size() > 256) return invalid;
    std::memcpy(next.text.data(), text.c_str(), text.size());
    next.textSize = text.size();
    destination = next;
    return {};
}
core::Result readUdpRules(JsonVariantConst rules, uint16_t globalPort, UdpRulesConfig &destination) {
    if (!rules.is<JsonArrayConst>() || rules.size() > 4) return {core::ErrorCode::InvalidArgument, "udp_rules"};
    UdpRulesConfig next;
    size_t index = 0;
    for (JsonVariantConst rule : rules.as<JsonArrayConst>()) {
        if (!rule.is<JsonObjectConst>() || !rule["enabled"].is<bool>() ||
            !rule["source"].is<const char *>()) return {core::ErrorCode::InvalidArgument, "udp_rule"};
        const JsonString name = rule["source"].as<JsonString>();
        if (std::string_view{name.c_str(), name.size()} == "udpPacket") {
            const auto valid = readUdpRule(rule["udp"], next[index]);
            if (!valid.ok()) return valid;
            next[index].enabled = rule["enabled"].as<bool>();
            if (next[index].enabled && globalPort != 0 && next[index].port == globalPort) {
                return {core::ErrorCode::ResourceConflict, "udp_global_port"};
            }
        }
        ++index;
    }
    destination = next;
    return {};
}
bool matchesUdpRule(const UdpRuleConfig &config, const std::array<uint8_t, 4> &ip,
                    uint16_t port, std::string_view text) {
    if (!config.enabled || (config.filterIp && ip != config.remoteIp) ||
        (config.remotePort && port != config.remotePort)) return false;
    const std::string_view expected{config.text.data(), config.textSize};
    return expected.empty() || (config.contains ? text.find(expected) != std::string_view::npos : text == expected);
}
const core::PeripheralDescriptor &UdpRules::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> UdpRules::properties() const { return descriptors; }
core::List<core::TriggerDescriptor> UdpRules::triggers() const { return conditions; }
core::Result UdpRules::begin(uint32_t now) { started_ = true; return tick(now); }
void UdpRules::stop() {
    for (uint8_t i = 0; i < 4; ++i) { hardware_.close(i); opened_[i] = false; }
    pending_.fill(0); started_ = false; health_ = {};
}
void UdpRules::configure(const UdpRulesConfig &config) {
    const bool started = started_;
    stop(); started_ = started; config_ = config; ports_.fill(0);
    counts_.fill(0); snapshots_.fill({});
    size_t count = 0;
    for (const auto &rule : config) {
        if (!rule.enabled) continue;
        bool duplicate = false;
        for (size_t i = 0; i < count; ++i) duplicate |= ports_[i] == rule.port;
        if (!duplicate) ports_[count++] = rule.port;
    }
}
core::Result UdpRules::tick(uint32_t now) {
    if (!started_) return {core::ErrorCode::Unavailable, "udp_rules"};
    core::Result error;
    if (!hardware_.connected()) {
        for (uint8_t i = 0; i < 4; ++i) { if (opened_[i]) hardware_.close(i); opened_[i] = false; }
        pending_.fill(0);
    }
    for (uint8_t socket = 0; socket < 4; ++socket) {
        if (!ports_[socket] || !hardware_.connected()) continue;
        core::Result result;
        if (!opened_[socket]) { result = hardware_.open(socket, ports_[socket]); opened_[socket] = result.ok(); }
        for (uint8_t packet = 0; packet < 4 && result.ok(); ++packet) {
            std::array<char, 256> data{};
            std::array<uint8_t, 4> ip{};
            size_t size = 0; uint16_t remotePort = 0; bool received = false;
            result = hardware_.receive(socket, data.data(), data.size(), size, ip, remotePort, received);
            if (!result.ok() || !received) break;
            result = size <= data.size() ? Transport::validateText({data.data(), size}) :
                core::Result{core::ErrorCode::CapacityExceeded, "udp_trigger_payload"};
            if (!result.ok()) break;
            for (uint8_t rule = 0; rule < 4; ++rule) {
                if (config_[rule].port != ports_[socket] ||
                    !matchesUdpRule(config_[rule], ip, remotePort, {data.data(), size})) continue;
                if (pending_[rule] == 4) { result = {core::ErrorCode::CapacityExceeded, "udp_trigger_queue"}; break; }
                ++pending_[rule]; ++counts_[rule];
                snapshots_[rule].updatedAtMs = now;
            }
        }
        if (!result.ok()) {
            hardware_.close(socket); opened_[socket] = false;
            if (error.ok()) error = result;
        }
    }
    for (uint8_t rule = 0; rule < 4; ++rule) {
        snapshots_[rule].value = counts_[rule];
        snapshots_[rule].quality = !config_[rule].enabled ? core::Quality::Disabled :
            !hardware_.connected() ? core::Quality::Unavailable : error.ok() ? core::Quality::Valid : core::Quality::Error;
        snapshots_[rule].error = error.code;
    }
    health_ = {error.ok(), error.code};
    return error;
}
uint8_t UdpRules::consume(uint8_t rule) {
    if (rule >= 4) return 0;
    const auto count = pending_[rule]; pending_[rule] = 0; return count;
}
core::Result UdpRules::readProperty(std::string_view id, core::PropertySnapshot &value) const {
    for (size_t i = 0; i < 4; ++i) if (id == descriptors[i].id) { value = snapshots_[i]; return {}; }
    return {core::ErrorCode::NotFound, "udp_rule_property"};
}
}
