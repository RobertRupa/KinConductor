#include "peripherals/transport.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace kc868a6::peripherals {
namespace {
const core::PeripheralDescriptor identity{"transport.main", "wifi-udp-uart"};
const std::array<core::PropertyDescriptor, 14> propertyList{{
    {"transport.main/wifi/connected", core::ValueType::Boolean, ""},
    {"transport.main/udp/rx/count", core::ValueType::UnsignedInteger, "messages"},
    {"transport.main/udp/rx/text", core::ValueType::Text, ""},
    {"transport.main/serial/1/rx/count", core::ValueType::UnsignedInteger, "frames"},
    {"transport.main/serial/1/rx/text", core::ValueType::Text, ""},
    {"transport.main/serial/2/rx/count", core::ValueType::UnsignedInteger, "frames"},
    {"transport.main/serial/2/rx/text", core::ValueType::Text, ""},
    {"transport.main/modbus/sequence", core::ValueType::UnsignedInteger, "transactions"},
    {"transport.main/modbus/last/value", core::ValueType::UnsignedInteger, "register", 60000},
    {"transport.main/modbus/last/ok", core::ValueType::Boolean, ""},
    {"transport.main/ebus/sequence", core::ValueType::UnsignedInteger, "transactions"},
    {"transport.main/ebus/last/value", core::ValueType::Number, "", 60000},
    {"transport.main/ebus/last/text", core::ValueType::Text, "", 60000},
    {"transport.main/ebus/connected", core::ValueType::Boolean, "", 5000},
}};
const std::array<core::TriggerDescriptor, 6> triggerList{{
    {"transport.main/wifi/condition", core::TriggerKind::Condition, propertyList[0].id},
    {"transport.main/udp/rx/condition", core::TriggerKind::Condition, propertyList[1].id},
    {"transport.main/serial/1/rx/condition", core::TriggerKind::Condition, propertyList[3].id},
    {"transport.main/serial/2/rx/condition", core::TriggerKind::Condition, propertyList[5].id},
    {"transport.main/modbus/condition", core::TriggerKind::Condition, propertyList[7].id},
    {"transport.main/ebus/condition", core::TriggerKind::Condition, propertyList[10].id},
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
const std::array<core::ParameterDescriptor, 4> modbusReadParameters{{
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 2.0},
    {"unit", core::ValueType::UnsignedInteger, true, 1.0, 247.0},
    {"address", core::ValueType::UnsignedInteger, true, 0.0, 65535.0},
    {"count", core::ValueType::UnsignedInteger, true, 1.0, 16.0},
}};
const std::array<core::ParameterDescriptor, 4> modbusWriteParameters{{
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 2.0},
    {"unit", core::ValueType::UnsignedInteger, true, 1.0, 247.0},
    {"address", core::ValueType::UnsignedInteger, true, 0.0, 65535.0},
    {"value", core::ValueType::UnsignedInteger, true, 0.0, 65535.0},
}};
const std::array<core::ParameterDescriptor, 4> modbusCoilParameters{{
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 2.0},
    {"unit", core::ValueType::UnsignedInteger, true, 1.0, 247.0},
    {"address", core::ValueType::UnsignedInteger, true, 0.0, 65535.0},
    {"on", core::ValueType::Boolean, true},
}};
const std::array<core::ParameterDescriptor, 5> ebusReadParameters{{
    {"host", core::ValueType::Text, true},
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 65535.0},
    {"circuit", core::ValueType::Text, true},
    {"name", core::ValueType::Text, true},
    {"field", core::ValueType::Text, true},
}};
const std::array<core::ParameterDescriptor, 5> ebusWriteParameters{{
    {"host", core::ValueType::Text, true},
    {"port", core::ValueType::UnsignedInteger, true, 1.0, 65535.0},
    {"circuit", core::ValueType::Text, true},
    {"name", core::ValueType::Text, true},
    {"value", core::ValueType::Text, true},
}};
const std::array<core::ActionDescriptor, 8> allActions{{
    actionList[0], actionList[1],
    {"transport.main/modbus/read-holding", modbusReadParameters},
    {"transport.main/modbus/read-input", modbusReadParameters},
    {"transport.main/modbus/write-holding", modbusWriteParameters},
    {"transport.main/modbus/write-coil", modbusCoilParameters},
    {"transport.main/ebus/read", ebusReadParameters},
    {"transport.main/ebus/write", ebusWriteParameters},
}};

bool isModbusAction(std::string_view id) {
    return id == allActions[2].id || id == allActions[3].id ||
           id == allActions[4].id || id == allActions[5].id;
}

bool isEbusAction(std::string_view id) {
    return id == allActions[6].id || id == allActions[7].id;
}

const uint32_t *unsignedParameter(core::List<core::Parameter> parameters,
                                  std::string_view id) {
    for (size_t index = 0; index < parameters.size; ++index) {
        if (parameters.data[index].id == id) {
            return std::get_if<uint32_t>(&parameters.data[index].value);
        }
    }
    return nullptr;
}

const std::string_view *textParameter(core::List<core::Parameter> parameters,
                                      std::string_view id) {
    for (size_t index = 0; index < parameters.size; ++index) {
        if (parameters.data[index].id == id) {
            return std::get_if<std::string_view>(&parameters.data[index].value);
        }
    }
    return nullptr;
}

bool validEbusToken(std::string_view token, size_t maximum) {
    return !token.empty() && token.size() <= maximum &&
        std::all_of(token.begin(), token.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '+';
        });
}

bool parseEbusNumber(std::string_view text, double &value) {
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\r' || text[start] == '\n')) ++start;
    size_t end = text.size();
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\r' || text[end - 1] == '\n')) --end;
    if (start == end) return false;
    std::array<char, Transport::maxEbusResponseBytes + 1> buffer{};
    const size_t size = end - start;
    if (size >= buffer.size()) return false;
    std::memcpy(buffer.data(), text.data() + start, size);
    char *finish = nullptr;
    value = std::strtod(buffer.data(), &finish);
    return finish != buffer.data() && *finish == '\0' && std::isfinite(value);
}

bool appendEbusText(std::array<char, Transport::maxEbusCommandBytes + 1> &buffer,
                    size_t &size, std::string_view text) {
    if (text.size() > Transport::maxEbusCommandBytes - size) return false;
    std::memcpy(buffer.data() + size, text.data(), text.size());
    size += text.size();
    buffer[size] = '\0';
    return true;
}
}

const core::PeripheralDescriptor &Transport::descriptor() const { return identity; }
core::List<core::PropertyDescriptor> Transport::properties() const { return propertyList; }
core::List<core::TriggerDescriptor> Transport::triggers() const { return triggerList; }
core::List<core::ActionDescriptor> Transport::actions() const { return allActions; }

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
        ? config_.serial1Enabled && !config_.serial1Reserved &&
              config_.serial1Protocol == rawSerialProtocol
        : config_.serial2Enabled && config_.serial2Protocol == rawSerialProtocol);
}

void Transport::refreshAvailability(uint32_t nowMs) {
    snapshots_[0] = {hardware_.wifiConnected(), config_.enabled
        ? core::Quality::Valid : core::Quality::Disabled, nowMs};
    for (size_t r = 0; r < receivers_.size(); ++r) {
        core::Quality quality = core::Quality::Valid;
        core::ErrorCode error = core::ErrorCode::None;
        if (!config_.enabled || (r == 0 && config_.udpListenPort == 0) ||
            (r == 1 && (!config_.serial1Enabled ||
                        config_.serial1Protocol != rawSerialProtocol)) ||
            (r == 2 && (!config_.serial2Enabled ||
                        config_.serial2Protocol != rawSerialProtocol))) {
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
    const bool modbusActive = config_.enabled &&
        ((config_.serial1Enabled && !config_.serial1Reserved &&
          config_.serial1Protocol == modbusRtuProtocol) ||
         (config_.serial2Enabled && config_.serial2Protocol == modbusRtuProtocol));
    if (!modbusActive) {
        for (size_t index = 7; index < 10; ++index) {
            snapshots_[index].quality = core::Quality::Disabled;
            snapshots_[index].error = core::ErrorCode::None;
        }
    } else if (snapshots_[7].quality == core::Quality::Disabled) {
        snapshots_[7] = {uint32_t{0}, core::Quality::Valid, nowMs};
        snapshots_[8] = {uint32_t{0}, core::Quality::WarmingUp, nowMs};
        snapshots_[9] = {false, core::Quality::WarmingUp, nowMs};
    }
    if (!config_.enabled) {
        for (size_t index = 10; index < snapshots_.size(); ++index) {
            snapshots_[index].quality = core::Quality::Disabled;
            snapshots_[index].error = core::ErrorCode::None;
        }
    } else {
        if (snapshots_[10].quality == core::Quality::Disabled) {
            snapshots_[10] = {uint32_t{0}, core::Quality::Valid, nowMs};
            snapshots_[11] = {0.0, core::Quality::WarmingUp, nowMs};
            snapshots_[12] = {std::string_view(ebusResponse_.data(), 0), core::Quality::Unavailable, nowMs};
        }
        if (!hardware_.wifiConnected()) {
            snapshots_[13].quality = core::Quality::Unavailable;
            snapshots_[13].error = core::ErrorCode::Unavailable;
            snapshots_[13].updatedAtMs = nowMs;
        }
    }
}

core::Result Transport::configure(const TransportConfig &config, uint32_t nowMs) {
    if (config.serialIdleMs == 0 || config.serialIdleMs > 60000) {
        return {core::ErrorCode::InvalidArgument, "serialIdleMs"};
    }
    if (config.serial1Protocol > modbusRtuProtocol ||
        config.serial2Protocol > modbusRtuProtocol) {
        return {core::ErrorCode::InvalidArgument, "serialProtocol"};
    }
    hardware_.cancelModbus();
    hardware_.cancelEbusCommand();
    modbusPending_ = false;
    ebusPending_ = false;
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
    snapshots_[7] = {uint32_t{0}, core::Quality::Disabled, nowMs};
    snapshots_[8] = {uint32_t{0}, core::Quality::Disabled, nowMs};
    snapshots_[9] = {false, core::Quality::Disabled, nowMs};
    snapshots_[10] = {uint32_t{0}, started_ ? core::Quality::Valid : core::Quality::Unavailable, nowMs};
    snapshots_[11] = {0.0, core::Quality::WarmingUp, nowMs};
    snapshots_[12] = {std::string_view(ebusResponse_.data(), 0), core::Quality::Unavailable, nowMs};
    snapshots_[13] = {false, core::Quality::Unavailable, nowMs};
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
    snapshots_[7] = {uint32_t{0}, core::Quality::Disabled, nowMs};
    snapshots_[8] = {uint32_t{0}, core::Quality::Disabled, nowMs};
    snapshots_[9] = {false, core::Quality::Disabled, nowMs};
    snapshots_[10] = {uint32_t{0}, core::Quality::Valid, nowMs};
    snapshots_[11] = {0.0, core::Quality::WarmingUp, nowMs};
    snapshots_[12] = {std::string_view(ebusResponse_.data(), 0), core::Quality::Unavailable, nowMs};
    snapshots_[13] = {false, core::Quality::WarmingUp, nowMs};
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
    hardware_.modbusTask();
    ModbusResponse response;
    if (modbusPending_ && hardware_.pollModbus(response)) {
        modbusPending_ = false;
        const auto nextSequence = std::get<uint32_t>(snapshots_[7].value) + 1U;
        snapshots_[7] = {nextSequence, core::Quality::Valid, nowMs};
        snapshots_[9] = {response.result.ok(), response.result.ok()
            ? core::Quality::Valid : core::Quality::Error, nowMs, response.result.code};
        if (response.result.ok() && response.count > 0) {
            snapshots_[8] = {static_cast<uint32_t>(response.values[0]),
                             core::Quality::Valid, nowMs};
        } else if (!response.result.ok()) {
            snapshots_[8].quality = core::Quality::Error;
            snapshots_[8].error = response.result.code;
            snapshots_[8].updatedAtMs = nowMs;
        }
        if (!response.result.ok() && first.ok()) first = response.result;
    }
    EbusResponse ebusResponse;
    if (ebusPending_ && hardware_.pollEbusResponse(ebusResponse)) {
        ebusPending_ = false;
        completeEbus(ebusResponse, nowMs);
        if (!ebusResponse.result.ok() && first.ok()) first = ebusResponse.result;
    }
    health_ = {true, first.code};
    return first;
}

void Transport::completeEbus(const EbusResponse &response, uint32_t nowMs) {
    const size_t size = std::min(response.size, maxEbusResponseBytes);
    if (size != 0) std::memcpy(ebusResponse_.data(), response.text.data(), size);
    ebusResponse_[size] = '\0';
    const std::string_view responseText(ebusResponse_.data(), size);
    ++std::get<uint32_t>(snapshots_[10].value);
    snapshots_[10] = {std::get<uint32_t>(snapshots_[10].value), core::Quality::Valid, nowMs};
    const bool protocolError = response.result.ok() && responseText.size() >= 4 &&
        responseText.substr(0, 4) == "ERR:";
    const bool succeeded = response.result.ok() && !protocolError;
    snapshots_[13] = {response.connected, core::Quality::Valid, nowMs,
                      response.connected ? core::ErrorCode::None : response.result.code};
    snapshots_[12] = {std::string_view(ebusResponse_.data(), size),
        size > 0 ? core::Quality::Valid : core::Quality::Unavailable,
        nowMs, response.result.code};
    double numericValue = 0.0;
    if (succeeded && parseEbusNumber(responseText, numericValue)) {
        snapshots_[11] = {numericValue, core::Quality::Valid, nowMs};
    } else {
        snapshots_[11].quality = succeeded ? core::Quality::Unavailable : core::Quality::Error;
        snapshots_[11].error = succeeded ? core::ErrorCode::None
            : (response.result.ok() ? core::ErrorCode::OperationFailed : response.result.code);
        snapshots_[11].updatedAtMs = nowMs;
    }
}

void Transport::stop() {
    hardware_.cancelModbus();
    hardware_.cancelEbusCommand();
    modbusPending_ = false;
    ebusPending_ = false;
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
    if (isEbusAction(id)) {
        const auto &descriptors = id == allActions[6].id ? ebusReadParameters : ebusWriteParameters;
        if (!args.data || args.size != descriptors.size()) {
            return {core::ErrorCode::InvalidArgument, id};
        }
        for (size_t index = 0; index < args.size; ++index) {
            bool found = false;
            for (size_t parameter = 0; parameter < descriptors.size(); ++parameter) {
                if (args.data[index].id == descriptors[parameter].id &&
                    core::valueMatchesParameter(args.data[index].value, descriptors[parameter])) {
                    found = true;
                    break;
                }
            }
            if (!found) return {core::ErrorCode::InvalidArgument, args.data[index].id};
        }
        const auto *host = textParameter(args, "host");
        const auto *circuit = textParameter(args, "circuit");
        const auto *name = textParameter(args, "name");
        const auto *field = textParameter(args, id == allActions[6].id ? "field" : "value");
        const auto *port = unsignedParameter(args, "port");
        if (!host || !circuit || !name || !field || !port ||
            !Transport::validateHost(*host).ok() ||
            !validEbusToken(*circuit, 32) || !validEbusToken(*name, 64) ||
            !validEbusToken(*field, 32)) {
            return {core::ErrorCode::InvalidArgument, "ebus.parameters"};
        }
        if (id == allActions[7].id && !config_.ebusWritesEnabled) {
            return {core::ErrorCode::Unavailable, "ebus.write_disabled"};
        }
        return {};
    }
    if (isModbusAction(id)) {
        const auto *port = unsignedParameter(args, "port");
        const auto *unit = unsignedParameter(args, "unit");
        const auto *address = unsignedParameter(args, "address");
        if (!args.data || args.size != 4 || !port || !unit || !address ||
            *port < 1 || *port > 2 || *unit < 1 || *unit > 247 || *address > 65535) {
            return {core::ErrorCode::InvalidArgument, id};
        }
        const auto &descriptors = id == allActions[2].id || id == allActions[3].id
            ? modbusReadParameters : id == allActions[5].id
                ? modbusCoilParameters : modbusWriteParameters;
        for (size_t index = 0; index < args.size; ++index) {
            bool found = false;
            for (size_t parameter = 0; parameter < descriptors.size(); ++parameter) {
                if (args.data[index].id == descriptors[parameter].id &&
                    core::valueMatchesParameter(args.data[index].value, descriptors[parameter])) {
                    found = true;
                    break;
                }
            }
            if (!found) return {core::ErrorCode::InvalidArgument, args.data[index].id};
        }
        const auto *count = unsignedParameter(args, "count");
        const auto *value = unsignedParameter(args, "value");
        const auto *on = [&]() -> const bool * {
            for (size_t index = 0; index < args.size; ++index) {
                if (args.data[index].id == "on") return std::get_if<bool>(&args.data[index].value);
            }
            return nullptr;
        }();
        if ((id == allActions[2].id || id == allActions[3].id) &&
            (!count || *count < 1 || *count > 16U)) {
            return {core::ErrorCode::InvalidArgument, "count"};
        }
        if (id == allActions[4].id && !value) return {core::ErrorCode::InvalidArgument, "value"};
        if (id == allActions[5].id && !on) return {core::ErrorCode::InvalidArgument, "on"};
        const uint8_t protocol = *port == 1 ? config_.serial1Protocol : config_.serial2Protocol;
        if (protocol != modbusRtuProtocol) return {core::ErrorCode::Unavailable, "modbus.disabled"};
        if (!config_.enabled || (*port == 1
                ? !config_.serial1Enabled || config_.serial1Reserved
                : !config_.serial2Enabled)) {
            return {core::ErrorCode::Unavailable, "modbus.port"};
        }
        return {};
    }
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
    if (isEbusAction(id)) {
        if (!started_ || !config_.enabled) return {{core::ErrorCode::Unavailable, identity.id}};
        if (!hardware_.wifiConnected()) return {{core::ErrorCode::Unavailable, "wifi.disconnected"}};
        if (ebusPending_) return {{core::ErrorCode::ResourceConflict, "ebus.busy"}};
        const auto host = *textParameter(args, "host");
        const auto circuit = *textParameter(args, "circuit");
        const auto name = *textParameter(args, "name");
        const auto data = *textParameter(args, id == allActions[6].id ? "field" : "value");
        const auto port = static_cast<uint16_t>(*unsignedParameter(args, "port"));
        std::array<char, maxEbusCommandBytes + 1> command{};
        size_t commandSize = 0;
        const bool write = id == allActions[7].id;
        const bool built = appendEbusText(command, commandSize,
            write ? "write -c " : "read -f -n -c ") &&
            appendEbusText(command, commandSize, circuit) &&
            appendEbusText(command, commandSize, " ") &&
            appendEbusText(command, commandSize, name) &&
            appendEbusText(command, commandSize, " ") &&
            appendEbusText(command, commandSize, data);
        if (!built) return {{core::ErrorCode::CapacityExceeded, "ebus.command"}};
        const auto started = hardware_.startEbusCommand(
            host, port, std::string_view(command.data(), commandSize));
        if (!started.ok()) return {started};
        ebusPending_ = true;
        return {{}, true, false};
    }
    if (isModbusAction(id)) {
        if (!started_ || !config_.enabled) return {{core::ErrorCode::Unavailable, identity.id}};
        if (modbusPending_) return {{core::ErrorCode::ResourceConflict, "modbus.busy"}};
        ModbusRequest request;
        request.port = static_cast<uint8_t>(*unsignedParameter(args, "port"));
        request.unitId = static_cast<uint8_t>(*unsignedParameter(args, "unit"));
        request.address = static_cast<uint16_t>(*unsignedParameter(args, "address"));
        if (id == allActions[2].id || id == allActions[3].id) {
            request.operation = id == allActions[2].id
                ? ModbusOperation::ReadHolding : ModbusOperation::ReadInput;
            request.count = static_cast<uint16_t>(*unsignedParameter(args, "count"));
        } else if (id == allActions[4].id) {
            request.operation = ModbusOperation::WriteHolding;
            request.value = static_cast<uint16_t>(*unsignedParameter(args, "value"));
        } else {
            request.operation = ModbusOperation::WriteCoil;
            for (size_t index = 0; index < args.size; ++index) {
                if (args.data[index].id == "on") {
                    request.value = std::get<bool>(args.data[index].value) ? 1 : 0;
                }
            }
        }
        const auto started = hardware_.startModbus(request);
        if (!started.ok()) return {started};
        modbusPending_ = true;
        return {{}, true, false};
    }
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
