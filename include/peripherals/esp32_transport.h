#pragma once

#include "peripherals/transport.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <HardwareSerial.h>
#include <WiFiUdp.h>

namespace kc868a6::peripherals {

// UART baud rates/pins and Wi-Fi lifetime remain owned by the application.
class Esp32TransportHardware final : public ITransportHardware {
public:
    Esp32TransportHardware(HardwareSerial &serial1, HardwareSerial &serial2)
        : serial1_(serial1), serial2_(serial2) {}
    bool wifiConnected() const override;
    core::Result listenUdp(uint16_t port) override;
    void stopUdp() override;
    core::Result receiveUdp(char *data, size_t capacity, size_t &size,
                            bool &received) override;
    core::ActionResult sendUdp(std::string_view host, uint16_t port,
                               std::string_view data) override;
    core::Result serialRead(uint8_t port, int &byte) override;
    core::ActionResult serialSend(uint8_t port, std::string_view data) override;

private:
    HardwareSerial &serial1_;
    HardwareSerial &serial2_;
    WiFiUDP listener_;
    uint16_t listenPort_ = 0;
};

}  // namespace kc868a6::peripherals
#endif
