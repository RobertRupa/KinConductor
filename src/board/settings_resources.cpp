#include "board/settings_resources.h"

namespace kc868a6::board {

core::Result planResources(const DeviceSettings &settings, BoardOptions options,
                           ResourceManager &destination) {
    const auto valid = config::validateSettings(settings);
    if (!valid.ok()) return valid;
    if (options.radio != Radio::None && options.radio != Radio::LoRa &&
        options.radio != Radio::Nrf24) {
        return {core::ErrorCode::InvalidArgument, "radio"};
    }
    if (options.p17 != P17Uart::Disabled && options.p17 != P17Uart::Uart1 &&
        options.p17 != P17Uart::Uart2) {
        return {core::ErrorCode::InvalidArgument, "p17"};
    }
    if (options.nfc != NfcTransport::Spi && options.nfc != NfcTransport::I2c) {
        return {core::ErrorCode::InvalidArgument, "pn532"};
    }
    ResourceManager candidate;
    core::Result result;
    const auto add = [&candidate, &result](ResourceKind kind, uint8_t number,
                                         const char *owner, PinUse use = PinUse::Input) {
        if (result.ok()) result = candidate.claim({kind, number, owner, use});
    };
    const auto uart = [&add](uint8_t controller, uint8_t tx, uint8_t rx,
                            const char *owner) {
        add(ResourceKind::Uart, controller, owner);
        add(ResourceKind::Gpio, tx, owner, PinUse::Output);
        add(ResourceKind::Gpio, rx, owner);
    };
    uart(0, gpio::usbSerialTx, gpio::usbSerialRx, "usb");
    if (options.rs232) uart(1, gpio::rs232Tx, gpio::rs232Rx, "rs232");
    if (options.rs485) uart(2, gpio::rs485Tx, gpio::rs485Rx, "rs485");
    if (options.p17 != P17Uart::Disabled) {
        uart(options.p17 == P17Uart::Uart1 ? 1 : 2, p17HostTx, p17HostRx, "p17");
    }
    add(ResourceKind::Gpio, gpio::i2cSda, "i2c", PinUse::Output);
    add(ResourceKind::Gpio, gpio::i2cScl, "i2c", PinUse::Output);
    add(ResourceKind::I2cAddress, i2c::relayExpanderAddress, "relays");
    add(ResourceKind::I2cAddress, i2c::inputExpanderAddress, "inputs");
    for (uint8_t pin : gpio::analogInputs) {
        add(ResourceKind::Gpio, pin, "adc", PinUse::AnalogInput);
    }
    // Shared bus signals belong to the controller; clients only claim their CS.
    add(ResourceKind::Gpio, gpio::spiSck, "spi", PinUse::Output);
    add(ResourceKind::Gpio, gpio::spiMosi, "spi", PinUse::Output);
    add(ResourceKind::Gpio, gpio::spiMiso, "spi");
    if (settings.gpio32SensorType != 0) {
        add(ResourceKind::Gpio, gpio::sensor1, "sensor1", PinUse::Output);
    }
    if (settings.gpio33SensorType != 0) {
        add(ResourceKind::Gpio, gpio::sensor2, "sensor2", PinUse::Output);
    }
    if (settings.s2InputEnabled) add(ResourceKind::Gpio, gpio::s2Button, "s2");
    if (settings.beeperEnabled) {
        add(ResourceKind::Gpio, settings.beeperPin, "beeper", PinUse::Output);
    }
    if (settings.fanEnabled) {
        add(ResourceKind::Gpio, settings.fanPin, "fan", PinUse::Output);
    }
    if (options.dac1) add(ResourceKind::Gpio, gpio::dac1, "dac1", PinUse::AnalogOutput);
    if (options.dac2) add(ResourceKind::Gpio, gpio::dac2, "dac2", PinUse::AnalogOutput);
    if (settings.displayEnabled) add(ResourceKind::I2cAddress, i2c::oledAddress, "oled");
    // These legacy drivers probe and initialize even when their reporting flag is off.
    add(ResourceKind::I2cAddress, i2c::rtcAddress, "rtc");
    add(ResourceKind::I2cAddress, i2c::bme680Address, "bme680");
    add(ResourceKind::I2cAddress, i2c::ens160Address, "ens160");
    add(ResourceKind::I2cAddress, i2c::qmc5883lAddress, "compass");
    if (settings.pn532Enabled) {
        if (options.nfc == NfcTransport::Spi) {
            add(ResourceKind::Gpio, gpio::pn532SpiCs, "pn532", PinUse::Output);
        } else {
            add(ResourceKind::I2cAddress, pn532I2cAddress, "pn532");
        }
    }
    if (options.radio != Radio::None) {
        add(ResourceKind::Gpio, gpio::spiCs, "radio", PinUse::Output);
        if (options.radio == Radio::LoRa) {
            add(ResourceKind::Gpio, gpio::loraReset, "lora", PinUse::Output);
            add(ResourceKind::Gpio, gpio::loraDio0, "lora");
        } else {
            add(ResourceKind::Gpio, gpio::nrf24Ce, "nrf24", PinUse::Output);
        }
    }
    if (!result.ok()) return result;
    destination = candidate;
    return {};
}

core::Result BoardSettingsValidator::validate(const DeviceSettings &settings) const {
    ResourceManager resources;
    return planResources(settings, options_, resources);
}

}  // namespace kc868a6::board
