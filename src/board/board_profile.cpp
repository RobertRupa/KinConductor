#include "board/board_profile.h"

namespace kc868a6::board {
namespace {

const std::array<PinDescriptor, pinCount> profile{{
    {gpio::s2Button, "S2 / BOOT", false, true, false, false},
    {gpio::usbSerialTx, "USB UART0 TX", false, false, false, false},
    {gpio::loraDio0, "LoRa DIO0", false, true, false, false},
    {gpio::usbSerialRx, "USB UART0 RX", false, false, false, false},
    {gpio::i2cSda, "I2C SDA", false, false, false, false},
    {gpio::spiCs, "LoRa / nRF24 CS", false, true, false, false},
    {p17HostTx, "P17 IO12 / optional host TX", false, true, false, false},
    {p17HostRx, "P17 IO13 / optional host RX / PN532 SPI CS", false, false, false, false},
    {gpio::rs485Rx, "RS485 UART2 RX", false, false, false, false},
    {gpio::i2cScl, "I2C SCL", false, true, false, false},
    {gpio::rs232Tx, "RS232 UART1 TX", false, false, false, false},
    {gpio::rs232Rx, "RS232 UART1 RX", false, false, false, false},
    {gpio::spiSck, "SPI SCK", false, false, false, false},
    {gpio::spiMiso, "SPI MISO", false, false, false, false},
    {gpio::loraReset, "LoRa RESET", false, false, false, false},
    {gpio::nrf24Ce, "nRF24 CE", false, false, false, false},
    {gpio::spiMosi, "SPI MOSI", false, false, false, false},
    {gpio::dac2, "DA2", false, false, false, true},
    {gpio::dac1, "DA1 / beeper PWM", false, false, false, true},
    {gpio::rs485Tx, "RS485 UART2 TX", false, false, false, false},
    {gpio::sensor1, "Sensor 1", false, false, true, false},
    {gpio::sensor2, "Sensor 2", false, false, true, false},
    {gpio::analogInputs[2], "A3 / ADC1", true, false, true, false},
    {gpio::analogInputs[3], "A4 / ADC1", true, false, true, false},
    {gpio::analogInputs[0], "A1 / ADC1", true, false, true, false},
    {gpio::analogInputs[1], "A2 / ADC1", true, false, true, false},
}};

}  // namespace

core::List<PinDescriptor> pins() { return profile; }

const PinDescriptor *findPin(uint8_t number) {
    for (const auto &pin : profile) {
        if (pin.number == number) return &pin;
    }
    return nullptr;
}

bool supports(const PinDescriptor &pin, PinUse use) {
    switch (use) {
        case PinUse::Input: return true;
        case PinUse::Output: return !pin.inputOnly;
        case PinUse::AnalogInput: return pin.adc;
        case PinUse::AnalogOutput: return pin.dac;
    }
    return false;
}

}  // namespace kc868a6::board
