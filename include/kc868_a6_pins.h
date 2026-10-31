#pragma once

#include <cstdint>

namespace kc868a6 {

namespace gpio {
constexpr uint8_t analogInputs[] = {36, 39, 34, 35};
constexpr uint8_t sensor1 = 32;
constexpr uint8_t sensor2 = 33;
constexpr uint8_t dac1 = 26;
constexpr uint8_t dac2 = 25;
constexpr uint8_t i2cSda = 4;
constexpr uint8_t i2cScl = 15;
constexpr uint8_t rs485Rx = 14;
constexpr uint8_t rs485Tx = 27;
constexpr uint8_t rs232Rx = 17;
constexpr uint8_t rs232Tx = 16;
constexpr uint8_t ttlRx = 12;
constexpr uint8_t ttlTx = 13;
constexpr uint8_t spiCs = 5;
constexpr uint8_t spiMosi = 23;
constexpr uint8_t spiMiso = 19;
constexpr uint8_t spiSck = 18;
constexpr uint8_t loraReset = 21;
constexpr uint8_t loraDio0 = 2;
constexpr uint8_t nrf24Ce = 22;
constexpr uint8_t pn532SpiCs = 13;
constexpr uint8_t s2Button = 0;
constexpr uint8_t bootButton = s2Button;
constexpr uint8_t usbSerialRx = 3;
constexpr uint8_t usbSerialTx = 1;
}

namespace i2c {
constexpr uint8_t inputExpanderAddress = 0x22;
constexpr uint8_t relayExpanderAddress = 0x24;
constexpr uint8_t oledAddress = 0x3C;
constexpr uint8_t rtcAddress = 0x68;
constexpr uint8_t bme680Address = 0x77;
constexpr uint8_t ens160Address = 0x53;
constexpr uint8_t qmc5883lAddress = 0x0D;
}

constexpr uint8_t relayCount = 6;
constexpr uint8_t digitalInputCount = 8;
constexpr uint8_t analogInputCount = 4;
constexpr bool relayActiveLow = true;

}