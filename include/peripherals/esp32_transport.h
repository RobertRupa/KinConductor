#pragma once

#include "peripherals/transport.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <HardwareSerial.h>
#include <ModbusRTU.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

namespace kc868a6::peripherals {

// UART baud rates/pins and Wi-Fi lifetime remain owned by the application.
class Esp32TransportHardware final : public ITransportHardware {
public:
    Esp32TransportHardware(HardwareSerial &serial1, HardwareSerial &serial2);
    bool wifiConnected() const override;
    core::Result listenUdp(uint16_t port) override;
    void stopUdp() override;
    core::Result receiveUdp(char *data, size_t capacity, size_t &size,
                            bool &received) override;
    core::ActionResult sendUdp(std::string_view host, uint16_t port,
                               std::string_view data) override;
    core::Result serialRead(uint8_t port, int &byte) override;
    core::ActionResult serialSend(uint8_t port, std::string_view data) override;
    core::Result startModbus(const ModbusRequest &request) override;
    void modbusTask() override;
    bool pollModbus(ModbusResponse &response) override;
    void cancelModbus() override;
    core::Result startEbusCommand(std::string_view host, uint16_t port,
                                  std::string_view command) override;
    bool pollEbusResponse(EbusResponse &response) override;
    void cancelEbusCommand() override;

private:
    struct ModbusState {
        ModbusResponse response{};
        ModbusRequest request{};
        bool ready = false;
        bool busy = false;
        bool cancelled = false;
    };
    struct EbusState {
        EbusResponse response{};
        size_t size = 0;
        uint32_t startedAtMs = 0;
        bool active = false;
        bool ready = false;
        bool previousWasNewline = false;
    };
    static bool modbusCallback1(Modbus::ResultCode event, uint16_t transactionId, void *data);
    static bool modbusCallback2(Modbus::ResultCode event, uint16_t transactionId, void *data);
    void completeModbus(size_t index, Modbus::ResultCode event);
    HardwareSerial &serial1_;
    HardwareSerial &serial2_;
    WiFiUDP listener_;
    WiFiClient ebusClient_;
    EbusState ebusState_{};
    std::array<ModbusRTU, 2> modbusClients_{};
    std::array<ModbusState, 2> modbusStates_{};
    std::array<bool, 2> modbusReady_{};
    uint16_t listenPort_ = 0;
    static Esp32TransportHardware *active_;
};

}  // namespace kc868a6::peripherals
#endif
