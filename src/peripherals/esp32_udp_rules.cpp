#include "peripherals/esp32_udp_rules.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
namespace kc868a6::peripherals {
bool Esp32UdpRulesHardware::connected() const { return WiFi.status() == WL_CONNECTED; }
core::Result Esp32UdpRulesHardware::open(uint8_t socket, uint16_t port) {
    if (socket >= sockets_.size() || !port) return {core::ErrorCode::InvalidArgument, "udp_rule_socket"};
    sockets_[socket].stop();
    return sockets_[socket].begin(port) == 1 ? core::Result{} :
        core::Result{core::ErrorCode::OperationFailed, "udp_rule_bind"};
}
void Esp32UdpRulesHardware::close(uint8_t socket) { if (socket < sockets_.size()) sockets_[socket].stop(); }
core::Result Esp32UdpRulesHardware::receive(uint8_t socket, char *data, size_t capacity, size_t &size,
                    std::array<uint8_t, 4> &ip, uint16_t &port, bool &received) {
    size = 0; received = false;
    if (socket >= sockets_.size()) return {core::ErrorCode::InvalidArgument, "udp_rule_socket"};
    auto &udp = sockets_[socket];
    const int bytes = udp.parsePacket();
    if (bytes < 0) return {core::ErrorCode::OperationFailed, "udp_rule_receive"};
    if (!bytes) return {};
    received = true;
    const auto remote = udp.remoteIP();
    for (uint8_t i = 0; i < 4; ++i) ip[i] = remote[i];
    port = udp.remotePort();
    if (static_cast<size_t>(bytes) > capacity) {
        udp.clear(); return {core::ErrorCode::CapacityExceeded, "udp_rule_payload"};
    }
    const int count = udp.read(reinterpret_cast<uint8_t *>(data), bytes);
    udp.clear();
    if (count != bytes) return {core::ErrorCode::OperationFailed, "udp_rule_read"};
    size = count; return {};
}
}
#endif
