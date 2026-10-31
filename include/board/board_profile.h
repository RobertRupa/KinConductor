#pragma once

#include "core/peripheral.h"
#include "kc868_a6_pins.h"

namespace kc868a6::board {

enum class PinUse { Input, Output, AnalogInput, AnalogOutput };

struct PinDescriptor {
    uint8_t number;
    const char *role;
    bool inputOnly;
    bool strapping;
    bool adc;
    bool dac;
};

core::List<PinDescriptor> pins();
const PinDescriptor *findPin(uint8_t number);
bool supports(const PinDescriptor &pin, PinUse use);

constexpr uint8_t uartCount = 3;
constexpr size_t pinCount = 26;
constexpr uint8_t relayPortMask = 0x3F;
constexpr uint8_t relayReservedPortMask = 0xC0;
constexpr uint8_t pn532I2cAddress = 0x24;

// Explicit host routing for future P17 profiles, not the legacy ttlRx/ttlTx aliases.
constexpr uint8_t p17HostTx = 12;
constexpr uint8_t p17HostRx = 13;

}  // namespace kc868a6::board
