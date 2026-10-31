#include "peripherals/transport.h"

#include <algorithm>
#include <cstring>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"transport.main", "wifi-udp-uart"};
const std::array<core::PropertyDescriptor, 7> propertyList{{
    {"transport.main/wifi/connected", core::ValueType::Boolean, ""},
    {"transport.main/udp/rx/count", core::ValueType::UnsignedInteger, "messages"},
    {"transport.main/udp/rx/text", core::ValueType::Text, ""},
    {"transport.main/serial/1/rx/count", core::ValueType::UnsignedInteger, "frames"},
    {"transport.main/serial/1/rx/text", core::ValueType::Text, ""},
    {"transport.main/serial/2/rx/count", core::ValueType::UnsignedInteger, "frames"},
    {"transport.main/serial/2/rx/text", core::ValueType::Text, ""},
}};
const std::array<core::TriggerDescriptor, 4> triggerList{{
    {"transport.main/wifi/condition", core::TriggerKind::Condition, propertyList[0].id},
    {"transport.main/udp/rx/condition", core::TriggerKind::Condition, propertyList[1].id},
    {"transport.main/serial/1/rx/condition", core::TriggerKind::Condition, propertyList[3].id},
    {"transport.main/serial/2/rx/condition", core::TriggerKind::Condition, propertyList[5].id},
}};
const std::array<core::ParameterDescriptor, 3> udpParameters{{
    {"host", core::ValueType::Text, true},
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 65535.0},
    {"data", core::ValueType::Text, true},
}};
const std::array<core::ParameterDescriptor, 2> serialParameters{{
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 2.0},
    {"data", core::ValueType::Text, true},
}};
const std::array<core::ActionDescriptor, 2> actionList{{
    {"transport.main/udp/send", udpParameters},
    {"transport.main/serial/send", serialParameters},
}};
}

const core::PeripheralDescriptor &Transport::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> Transport::properties() const { return propertyList; }
core::List<core::TriggerDescriptor> Transport::triggers() const { return triggerList; }
core::List<core::ActionDescriptor> Transport::actions() const { return actionList; }

core::Result Transport::validateText(std::string_view text) {
    if (text.size() > maxPayloadBytes) return {core::ErrorCode::CapacityExceeded, "data"};
    for (size_t i = 0; i < text.size();) {
        const auto byte = static_cast<uint8_t>(text[i++]);
        if (byte < 0x80) {
            if ((byte < 0x20 && byte != '\t' && byte != '\r' && byte != '\n') ||
                byte == 0x7F) return {core::ErrorCode::InvalidArgument, "data.utf8"};
            continue;
        }
        unsigned following = 0;
        uint32_t codepoint = 0;
        uint32_t minimum = 0;
        if (byte >= 0xC2 && byte <= 0xDF) {
            following = 1; codepoint = byte & 0x1F; minimum = 0x80;
        } else if (byte >= 0xE0 && byte <= 0xEF) {
            following = 2; codepoint = byte & 0x0F; minimum = 0x800;
        } else if (byte >= 0xF0 && byte <= 0xF4) {
            following = 3; codepoint = byte & 7; minimum = 0x10000;
        } else return {core::ErrorCode::InvalidArgument, "data.utf8"};
        if (following > text.size() - i) return {core::ErrorCode::InvalidArgument, "data.utf8"};
        for (unsigned j = 0; j < following; ++j) {
            const auto next = static_cast<uint8_t>(text[i++]);
            if ((next & 0xC0) != 0x80) return {core::ErrorCode::InvalidArgument, "data.utf8"};
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            (codepoint >= 0x80 && codepoint <= 0x9F)) {
            return {core::ErrorCode::InvalidArgument, "data.utf8"};
        }
    }
    return {};
}

core::Result Transport::validateHost(std::string_view host) {
    if (host.size() > maxHostBytes) return {core::ErrorCode::CapacityExceeded, "host"};
    if (host.empty()) return {core::ErrorCode::InvalidArgument, "host"};
    size_t labelSize = 0;
    char previous = '.';
    for (char c : host) {
        if (c == '.') {
            if (labelSize == 0 || previous == '-') return {core::ErrorCode::InvalidArgument, "host"};
            labelSize = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-') ||
                (labelSize == 0 && c == '-') || ++labelSize > 63) {
                return {core::ErrorCode::InvalidArgument, "host"};
            }
        }
        previous = c;
    }
    return labelSize != 0 && previous != '-' ? core::Result{}
        : core::Result{core::ErrorCode::InvalidArgument, "host"};
}

bool Transport::serialEnabled(uint8_t port) const {
    return config_.enabled && (port == 1
        ? config_.serial1Enabled && !config_.serial1Reserved : config_.serial2Enabled);
}

void Transport::refreshAvailability(uint32_t nowMs) {
    snapshots_[0] = {hardware_.wifiConnected(), config_.enabled
        ? core::Quality::Valid : core::Quality::Disabled, nowMs};
    for (size_t r = 0; r < receivers_.size(); ++r) {
        core::Quality quality = core::Quality::Valid;
        core::ErrorCode error = core::ErrorCode::None;
        if (!config_.enabled || (r == 0 && config_.udpListenPort == 0) ||
            (r == 1 && !config_.serial1Enabled) || (r == 2 && !config_.serial2Enabled)) {
            quality = core::Quality::Disabled;
        } else if ((r == 0 && !hardware_.wifiConnected()) ||
                   (r == 1 && config_.serial1Reserved)) {
            quality = core::Quality::Unavailable;
            error = r == 1 ? core::ErrorCode::ResourceConflict : core::ErrorCode::Unavailable;
        }
        for (size_t p = 1 + r * 2; p <= 2 + r * 2; ++p) {
            // Keep a receive error visible until a valid frame/reconfiguration.
            if (quality != core::Quality::Valid || snapshots_[p].quality != core::Quality::Error) {
                snapshots_[p].quality = quality;
                snapshots_[p].error = error;
            }
        }
    }
}

core::Result Transport::configure(const TransportConfig &config, uint32_t nowMs) {
    if (config.serialIdleMs == 0 || config.serialIdleMs > 60000) {
        return {core::ErrorCode::InvalidArgument, "serialIdleMs"};
    }
    hardware_.stopUdp();
    listening_ = false;
    config_ = config;
    for (auto &receiver : receivers_) {
        receiver.frameSize = 0;
        receiver.overflow = false;
    }
    for (auto &snapshot : snapshots_) {
        snapshot.quality = started_ ? core::Quality::Valid : core::Quality::Unavailable;
        snapshot.error = core::ErrorCode::None;
    }
    if (!started_) return {};
    refreshAvailability(nowMs);
    const auto result = updateListener();
    if (!result.ok()) receiverError(0, result.code, nowMs);
    health_ = {true, result.code};
    return result;
}

core::Result Transport::begin(uint32_t nowMs) {
    if (started_) return {};
    receivers_ = {};
    snapshots_ = {};
    for (size_t r = 0; r < receivers_.size(); ++r) {
        snapshots_[1 + r * 2].value = uint32_t{0};
        snapshots_[2 + r * 2].value = std::string_view(receivers_[r].last.data(), 0);
    }
    started_ = true;
    refreshAvailability(nowMs);
    health_ = {true, core::ErrorCode::None};
    // Binding happens in tick: registry startup must not depend on Wi-Fi/DHCP.
    return {};
}

core::Result Transport::updateListener() {
    const bool wanted = config_.enabled && config_.udpListenPort != 0 && hardware_.wifiConnected();
    if (!wanted) {
        if (listening_) hardware_.stopUdp();
        listening_ = false;
        return {};
    }
    if (listening_) return {};
    const auto result = hardware_.listenUdp(config_.udpListenPort);
    listening_ = result.ok();
    return result;
}

void Transport::receiverError(size_t receiver, core::ErrorCode error, uint32_t nowMs) {
    for (size_t p = 1 + receiver * 2; p <= 2 + receiver * 2; ++p) {
        snapshots_[p].quality = core::Quality::Error;
        snapshots_[p].error = error;
        snapshots_[p].updatedAtMs = nowMs;
    }
}

core::Result Transport::accept(size_t receiver, std::string_view text, uint32_t nowMs) {
    const auto result = validateText(text);
    if (!result.ok()) {
        receiverError(receiver, result.code, nowMs);
        return result;
    }
    auto &rx = receivers_[receiver];
    if (!text.empty()) std::memcpy(rx.last.data(), text.data(), text.size());
    rx.lastSize = text.size();
    ++rx.sequence;  // Unsigned wrap is intentional; conditions use numeric !=.
    snapshots_[1 + receiver * 2] = {rx.sequence, core::Quality::Valid, nowMs};
    snapshots_[2 + receiver * 2] = {
        std::string_view(rx.last.data(), rx.lastSize), core::Quality::Valid, nowMs};
    return {};
}

core::Result Transport::finishSerial(uint8_t port, uint32_t nowMs) {
    auto &rx = receivers_[port];
    size_t size = rx.frameSize;
    if (size != 0 && rx.frame[size - 1] == '\r') --size;
    const auto result = rx.overflow
        ? core::Result{core::ErrorCode::CapacityExceeded, "serial.rx.overflow"}
        : accept(port, std::string_view(rx.frame.data(), size), nowMs);
    if (!result.ok()) receiverError(port, result.code, nowMs);
    rx.frameSize = 0;
    rx.overflow = false;
    return result;
}

core::Result Transport::pollSerial(uint8_t port, uint32_t nowMs) {
    auto &rx = receivers_[port];
    core::Result first;
    bool exhausted = false;
    for (size_t n = 0; n < serialReadBudget; ++n) {
        int byte = -1;
        auto result = hardware_.serialRead(port, byte);
        if (!result.ok() || byte < -1 || byte > 255) {
            if (result.ok()) result = {core::ErrorCode::OperationFailed, "serial.rx.byte"};
            receiverError(port, result.code, nowMs);
            rx.frameSize = 0;
            rx.overflow = false;
            return result;
        }
        if (byte == -1) { exhausted = true; break; }
        rx.lastByteMs = nowMs;
        if (byte == '\n') {
            result = finishSerial(port, nowMs);
            if (!result.ok() && first.ok()) first = result;
        } else if (!rx.overflow) {
            if (rx.frameSize < rx.frame.size()) {
                rx.frame[rx.frameSize++] = static_cast<char>(byte);
                // A CR is allowed as byte 257 only as the CRLF delimiter.
                if (rx.frameSize > maxPayloadBytes && byte != '\r') rx.overflow = true;
            } else rx.overflow = true;
            if (rx.overflow) {
                receiverError(port, core::ErrorCode::CapacityExceeded, nowMs);
                if (first.ok()) first = {core::ErrorCode::CapacityExceeded, "serial.rx.overflow"};
            }
        }
    }
    if (exhausted && (rx.frameSize != 0 || rx.overflow) &&
        static_cast<uint32_t>(nowMs - rx.lastByteMs) >= config_.serialIdleMs) {
        const auto result = finishSerial(port, nowMs);
        if (!result.ok() && first.ok()) first = result;
    }
    return first;
}

core::Result Transport::tick(uint32_t nowMs) {
    if (!started_) return {core::ErrorCode::Unavailable, identity.id};
    refreshAvailability(nowMs);
    core::Result first = updateListener();
    if (!first.ok()) receiverError(0, first.code, nowMs);
    if (listening_) {
        std::array<char, maxPayloadBytes> buffer{};
        size_t size = 0;
        bool received = false;
        auto result = hardware_.receiveUdp(buffer.data(), buffer.size(), size, received);
        if (result.ok() && received) {
            result = size > buffer.size()
                ? core::Result{core::ErrorCode::CapacityExceeded, "udp.rx.overflow"}
                : accept(0, std::string_view(buffer.data(), size), nowMs);
        }
        if (!result.ok()) {
            receiverError(0, result.code, nowMs);
            if (first.ok()) first = result;
        }
    }
    for (uint8_t port = 1; port <= 2; ++port) {
        if (!serialEnabled(port)) continue;
        const auto result = pollSerial(port, nowMs);
        if (!result.ok() && first.ok()) first = result;
    }
    health_ = {true, first.code};
    return first;
}

void Transport::stop() {
    hardware_.stopUdp();
    listening_ = false;
    started_ = false;
    health_ = {};
    for (auto &snapshot : snapshots_) {
        snapshot.quality = core::Quality::Unavailable;
        snapshot.error = core::ErrorCode::Unavailable;
    }
    for (auto &rx : receivers_) { rx.frameSize = 0; rx.overflow = false; }
}

core::Result Transport::readProperty(std::string_view id, core::PropertySnapshot &snapshot) const {
    snapshot = {};
    for (size_t p = 0; p < propertyList.size(); ++p) {
        if (id == propertyList[p].id) { snapshot = snapshots_[p]; return {}; }
    }
    return {core::ErrorCode::NotFound, id};
}

core::Result Transport::validate(std::string_view id, core::List<core::Parameter> args) const {
    const bool udp = id == actionList[0].id;
    if (!udp && id != actionList[1].id) return {core::ErrorCode::NotFound, id};
    if (!args.data || args.size != (udp ? 3U : 2U)) {
        return {core::ErrorCode::InvalidArgument, id};
    }
    const uint32_t *port = nullptr;
    const std::string_view *data = nullptr;
    const std::string_view *host = nullptr;
    for (size_t n = 0; n < args.size; ++n) {
        const auto &arg = args.data[n];
        if (arg.id == "port" && !port) port = std::get_if<uint32_t>(&arg.value);
        else if (arg.id == "data" && !data) data = std::get_if<std::string_view>(&arg.value);
        else if (udp && arg.id == "host" && !host) host = std::get_if<std::string_view>(&arg.value);
        else return {core::ErrorCode::InvalidArgument, arg.id};
    }
    if (!port || *port < 1 || *port > (udp ? 65535U : 2U) || !data || (udp && !host)) {
        return {core::ErrorCode::InvalidArgument, id};
    }
    const auto result = validateText(*data);
    if (!result.ok()) return result;
    return udp ? validateHost(*host) : core::Result{};
}

core::ActionResult Transport::execute(std::string_view id, core::List<core::Parameter> args) {
    const auto result = validate(id, args);
    if (!result.ok()) return {result};
    const bool udp = id == actionList[0].id;
    uint32_t port = 0;
    std::string_view data;
    std::string_view host;
    for (size_t n = 0; n < args.size; ++n) {
        const auto &arg = args.data[n];
        if (arg.id == "port") port = std::get<uint32_t>(arg.value);
        else if (arg.id == "data") data = std::get<std::string_view>(arg.value);
        else if (arg.id == "host") host = std::get<std::string_view>(arg.value);
    }
    if (!started_ || !config_.enabled) return {{core::ErrorCode::Unavailable, identity.id}};
    if (udp) {
        if (!hardware_.wifiConnected()) return {{core::ErrorCode::Unavailable, "wifi.disconnected"}};
        return hardware_.sendUdp(host, static_cast<uint16_t>(port), data);
    }
    if (port == 1 && config_.serial1Reserved) {
        return {{core::ErrorCode::ResourceConflict, "serial.1.reserved"}};
    }
    if (!serialEnabled(static_cast<uint8_t>(port))) {
        return {{core::ErrorCode::Unavailable, "serial.disabled"}};
    }
    return hardware_.serialSend(static_cast<uint8_t>(port), data);
}

}  // namespace kc868a6::peripherals
