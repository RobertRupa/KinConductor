#include "peripherals/esp32_transport.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <algorithm>
#include <cstring>

namespace kc868a6::peripherals {
namespace {
void discardPacket(WiFiUDP &udp) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    udp.clear();
#else
    udp.flush();
#endif
}
}

Esp32TransportHardware *Esp32TransportHardware::active_ = nullptr;

Esp32TransportHardware::Esp32TransportHardware(HardwareSerial &serial1,
                                               HardwareSerial &serial2)
    : serial1_(serial1), serial2_(serial2) {
    active_ = this;
}

bool Esp32TransportHardware::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }

core::Result Esp32TransportHardware::listenUdp(uint16_t port) {
    stopUdp();
    if (port == 0) return {};
    if (!wifiConnected()) return {core::ErrorCode::Unavailable, "wifi.disconnected"};
    if (listener_.begin(port) != 1) {
        return {core::ErrorCode::OperationFailed, "udp.listen"};
    }
    listenPort_ = port;
    return {};
}

void Esp32TransportHardware::stopUdp() {
    listener_.stop();
    listenPort_ = 0;
}

core::Result Esp32TransportHardware::receiveUdp(char *data, size_t capacity,
                                               size_t &size, bool &received) {
    size = 0;
    received = false;
    if (!data || capacity == 0) return {core::ErrorCode::InvalidArgument, "udp.rx.buffer"};
    if (!wifiConnected()) return {core::ErrorCode::Unavailable, "wifi.disconnected"};
    if (listenPort_ == 0) return {core::ErrorCode::Unavailable, "udp.listen.disabled"};
    const int packetSize = listener_.parsePacket();
    if (packetSize < 0) return {core::ErrorCode::OperationFailed, "udp.rx.parse"};
    // ESP32 WiFiUDP returns 0 for both no packet and a zero-length datagram.
    if (packetSize == 0) return {};
    received = true;
    if (static_cast<size_t>(packetSize) > capacity) {
        discardPacket(listener_);
        return {core::ErrorCode::CapacityExceeded, "udp.rx.overflow"};
    }
    const int read = listener_.read(reinterpret_cast<unsigned char *>(data), packetSize);
    discardPacket(listener_);
    if (read != packetSize) return {core::ErrorCode::OperationFailed, "udp.rx.read"};
    size = static_cast<size_t>(read);
    return {};
}

core::ActionResult Esp32TransportHardware::sendUdp(std::string_view host, uint16_t port,
                                                  std::string_view data) {
    auto result = Transport::validateHost(host);
    if (!result.ok()) return {result};
    result = Transport::validateText(data);
    if (!result.ok()) return {result};
    if (port == 0) return {{core::ErrorCode::InvalidArgument, "port"}};
    if (!wifiConnected()) return {{core::ErrorCode::Unavailable, "wifi.disconnected"}};
    std::array<char, Transport::maxHostBytes + 1> hostname{};
    std::memcpy(hostname.data(), host.data(), host.size());
    IPAddress destination;
    const bool numeric = std::all_of(host.begin(), host.end(),
        [](char c) { return (c >= '0' && c <= '9') || c == '.'; });
    if (numeric) {
        if (!destination.fromString(hostname.data())) {
            return {{core::ErrorCode::InvalidArgument, "udp.host.parse"}};
        }
    } else if (WiFi.hostByName(hostname.data(), destination) != 1) {
        return {{core::ErrorCode::OperationFailed, "udp.host.dns"}};
    }
    if (!wifiConnected()) return {{core::ErrorCode::Unavailable, "wifi.disconnected"}};
    WiFiUDP sender;
    if (sender.beginPacket(destination, port) != 1) {
        return {{core::ErrorCode::OperationFailed, "udp.tx.begin"}};
    }
    const size_t written = sender.write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    if (written != data.size()) {
        sender.stop();
        return {{core::ErrorCode::OperationFailed, "udp.tx.write"}};
    }
    if (!wifiConnected()) {
        sender.stop();
        return {{core::ErrorCode::Unavailable, "wifi.disconnected"}};
    }
    const bool sent = sender.endPacket() == 1;
    sender.stop();
    if (!sent) return {{core::ErrorCode::OperationFailed, "udp.tx.end"}};
    if (!wifiConnected()) {
        return {{core::ErrorCode::Unavailable, "wifi.disconnected"}, true, false};
    }
    // Successful local enqueue is not an acknowledgement from the remote peer.
    return {{}, true, false};
}

core::Result Esp32TransportHardware::serialRead(uint8_t port, int &byte) {
    byte = -1;
    if (port < 1 || port > 2) return {core::ErrorCode::InvalidArgument, "port"};
    auto &serial = port == 1 ? serial1_ : serial2_;
    if (serial.available() > 0) byte = serial.read();
    return {};
}

core::ActionResult Esp32TransportHardware::serialSend(uint8_t port, std::string_view data) {
    if (port < 1 || port > 2) return {{core::ErrorCode::InvalidArgument, "port"}};
    const auto result = Transport::validateText(data);
    if (!result.ok()) return {result};
    auto &serial = port == 1 ? serial1_ : serial2_;
    if (serial.availableForWrite() < static_cast<int>(data.size())) {
        return {{core::ErrorCode::CapacityExceeded, "serial.tx.buffer"}};
    }
    const size_t written = serial.write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    if (written != data.size()) {
        return {{core::ErrorCode::OperationFailed, "serial.tx.write"}, written != 0, false};
    }
    return {{}, true, false};
}

core::Result Esp32TransportHardware::startModbus(const ModbusRequest &request) {
    if (request.port < 1 || request.port > 2 || request.unitId < 1 || request.unitId > 247) {
        return {core::ErrorCode::InvalidArgument, "modbus.request"};
    }
    const size_t index = request.port - 1;
    auto &state = modbusStates_[index];
    if (state.busy) return {core::ErrorCode::ResourceConflict, "modbus.busy"};
    if (!modbusReady_[index]) {
        auto &serial = request.port == 1 ? serial1_ : serial2_;
        if (!modbusClients_[index].begin(&serial)) {
            return {core::ErrorCode::OperationFailed, "modbus.begin"};
        }
        modbusClients_[index].master();
        modbusReady_[index] = true;
    }
    state = {};
    state.request = request;
    state.response.port = request.port;
    state.busy = true;
    uint16_t transaction = 0;
    switch (request.operation) {
        case ModbusOperation::ReadHolding:
            transaction = modbusClients_[index].readHreg(
                request.unitId, request.address, state.response.values.data(), request.count,
                index == 0 ? modbusCallback1 : modbusCallback2);
            break;
        case ModbusOperation::ReadInput:
            transaction = modbusClients_[index].readIreg(
                request.unitId, request.address, state.response.values.data(), request.count,
                index == 0 ? modbusCallback1 : modbusCallback2);
            break;
        case ModbusOperation::WriteHolding:
            transaction = modbusClients_[index].writeHreg(
                request.unitId, request.address, request.value,
                index == 0 ? modbusCallback1 : modbusCallback2);
            break;
        case ModbusOperation::WriteCoil:
            transaction = modbusClients_[index].writeCoil(
                request.unitId, request.address, request.value != 0,
                index == 0 ? modbusCallback1 : modbusCallback2);
            break;
        default:
            state.busy = false;
            return {core::ErrorCode::InvalidArgument, "modbus.operation"};
    }
    if (transaction == 0) {
        state.busy = false;
        return {core::ErrorCode::OperationFailed, "modbus.submit"};
    }
    return {};
}

void Esp32TransportHardware::modbusTask() {
    for (size_t index = 0; index < modbusClients_.size(); ++index) {
        if (modbusReady_[index]) modbusClients_[index].task();
    }
}

bool Esp32TransportHardware::pollModbus(ModbusResponse &response) {
    for (auto &state : modbusStates_) {
        if (!state.ready) continue;
        response = state.response;
        state.ready = false;
        return true;
    }
    return false;
}

void Esp32TransportHardware::cancelModbus() {
    for (auto &state : modbusStates_) {
        if (state.busy) state.cancelled = true;
        state.ready = false;
    }
}

core::Result Esp32TransportHardware::startEbusCommand(std::string_view host,
                                                       uint16_t port,
                                                       std::string_view command) {
    if (ebusState_.active || ebusState_.ready) {
        return {core::ErrorCode::ResourceConflict, "ebus.busy"};
    }
    if (!wifiConnected()) return {core::ErrorCode::Unavailable, "wifi.disconnected"};
    if (port == 0 || host.empty() || command.empty() ||
        command.size() > Transport::maxEbusCommandBytes) {
        return {core::ErrorCode::InvalidArgument, "ebus.request"};
    }
    std::array<char, Transport::maxHostBytes + 1> hostname{};
    std::memcpy(hostname.data(), host.data(), host.size());
    ebusClient_.stop();
    ebusClient_.setTimeout(1);
    if (!ebusClient_.connect(hostname.data(), port, 250)) {
        ebusClient_.stop();
        return {core::ErrorCode::Unavailable, "ebus.connect"};
    }
    if (ebusClient_.write(reinterpret_cast<const uint8_t *>(command.data()), command.size()) != command.size() ||
        ebusClient_.write(static_cast<uint8_t>('\n')) != 1) {
        ebusClient_.stop();
        return {core::ErrorCode::OperationFailed, "ebus.write"};
    }
    ebusState_ = {};
    ebusState_.active = true;
    ebusState_.startedAtMs = millis();
    ebusState_.response.connected = true;
    return {};
}

bool Esp32TransportHardware::pollEbusResponse(EbusResponse &response) {
    if (ebusState_.ready) {
        response = ebusState_.response;
        ebusState_.ready = false;
        return true;
    }
    if (!ebusState_.active) return false;
    constexpr size_t receiveBudget = 128;
    for (size_t count = 0; count < receiveBudget && ebusClient_.available() > 0; ++count) {
        const int received = ebusClient_.read();
        if (received < 0) break;
        const char byte = static_cast<char>(received);
        if (byte == '\n' && ebusState_.previousWasNewline) {
            ebusState_.response.size = ebusState_.size;
            ebusState_.response.result = {};
            if (ebusState_.size >= 4 &&
                std::memcmp(ebusState_.response.text.data(), "ERR:", 4) == 0) {
                ebusState_.response.result = {core::ErrorCode::OperationFailed, "ebus.command"};
            }
            ebusState_.active = false;
            ebusState_.ready = true;
            ebusClient_.stop();
            response = ebusState_.response;
            ebusState_.ready = false;
            return true;
        }
        ebusState_.previousWasNewline = byte == '\n';
        if (byte == '\r') continue;
        if (ebusState_.size == ebusState_.response.text.size() - 1) {
            ebusState_.response.result = {core::ErrorCode::CapacityExceeded, "ebus.response"};
            ebusState_.response.connected = true;
            ebusState_.active = false;
            ebusState_.ready = true;
            ebusClient_.stop();
            response = ebusState_.response;
            ebusState_.ready = false;
            return true;
        }
        ebusState_.response.text[ebusState_.size++] = byte;
    }
    if (static_cast<uint32_t>(millis() - ebusState_.startedAtMs) >= 2500 ||
        !ebusClient_.connected()) {
        ebusState_.response.result = {core::ErrorCode::OperationFailed, "ebus.timeout"};
        ebusState_.response.connected = false;
        ebusState_.response.size = ebusState_.size;
        ebusState_.active = false;
        ebusState_.ready = true;
        ebusClient_.stop();
        response = ebusState_.response;
        ebusState_.ready = false;
        return true;
    }
    return false;
}

void Esp32TransportHardware::cancelEbusCommand() {
    ebusClient_.stop();
    ebusState_ = {};
}

bool Esp32TransportHardware::modbusCallback1(Modbus::ResultCode event, uint16_t, void *) {
    if (active_) active_->completeModbus(0, event);
    return true;
}

bool Esp32TransportHardware::modbusCallback2(Modbus::ResultCode event, uint16_t, void *) {
    if (active_) active_->completeModbus(1, event);
    return true;
}

void Esp32TransportHardware::completeModbus(size_t index, Modbus::ResultCode event) {
    auto &state = modbusStates_[index];
    state.busy = false;
    if (state.cancelled) {
        state.cancelled = false;
        return;
    }
    state.response.result = event == Modbus::EX_SUCCESS
        ? core::Result{} : core::Result{core::ErrorCode::OperationFailed, "modbus.response"};
    state.response.count = state.response.result.ok() &&
            (state.request.operation == ModbusOperation::ReadHolding ||
             state.request.operation == ModbusOperation::ReadInput)
        ? state.request.count : 0;
    state.ready = true;
}

}  // namespace kc868a6::peripherals
#endif
