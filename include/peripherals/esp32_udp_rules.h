#pragma once
#include "peripherals/udp_rules.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFiUdp.h>
namespace kc868a6::peripherals {
class Esp32UdpRulesHardware final : public IUdpRulesHardware {
public:
    bool connected() const override;
    core::Result open(uint8_t socket, uint16_t port) override;
    void close(uint8_t socket) override;
    core::Result receive(uint8_t socket, char *data, size_t capacity, size_t &size,
                        std::array<uint8_t, 4> &ip, uint16_t &port, bool &received) override;
private:
    std::array<WiFiUDP, 4> sockets_;
};
}
#endif
