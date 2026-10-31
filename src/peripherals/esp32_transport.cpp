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

}  // namespace kc868a6::peripherals
#endif
