#pragma once

#include "board/resource_manager.h"
#include "config/config_service.h"

namespace kc868a6::board {

enum class Radio { None, LoRa, Nrf24 };
enum class P17Uart { Disabled, Uart1, Uart2 };
enum class NfcTransport { Spi, I2c };

struct BoardOptions {
    Radio radio = Radio::None;
    P17Uart p17 = P17Uart::Disabled;
    bool rs232 = true;
    bool rs485 = true;
    bool dac1 = false;
    bool dac2 = false;
    NfcTransport nfc = NfcTransport::Spi;
};

constexpr BoardOptions compiledBoardOptions() {
    BoardOptions options;
#if defined(USE_RADIO_NRF24) && defined(USE_RADIO_LORA)
#error "Select only one radio: nRF24 and SX1278 share the board CS pin"
#elif defined(USE_RADIO_NRF24)
    options.radio = Radio::Nrf24;
#elif defined(USE_RADIO_LORA)
    options.radio = Radio::LoRa;
#endif

#if defined(KC868_BOARD_P17_UART1) && defined(KC868_BOARD_P17_UART2)
#error "Select only one P17 UART routing option"
#elif defined(KC868_BOARD_P17_UART1)
    options.p17 = P17Uart::Uart1;
#elif defined(KC868_BOARD_P17_UART2)
    options.p17 = P17Uart::Uart2;
#endif

#if defined(KC868_BOARD_DISABLE_RS232)
    options.rs232 = false;
#endif
#if defined(KC868_BOARD_DISABLE_RS485)
    options.rs485 = false;
#endif

#if defined(KC868_BOARD_ENABLE_DAC1)
    options.dac1 = true;
#endif
#if defined(KC868_BOARD_ENABLE_DAC2)
    options.dac2 = true;
#endif

#if defined(KC868_BOARD_PN532_I2C) && defined(KC868_BOARD_PN532_SPI)
#error "Select only one PN532 transport option"
#elif defined(KC868_BOARD_PN532_I2C)
    options.nfc = NfcTransport::I2c;
#elif defined(KC868_BOARD_PN532_SPI)
    options.nfc = NfcTransport::Spi;
#endif

    return options;
}

core::Result planResources(const DeviceSettings &settings, BoardOptions options,
                           ResourceManager &destination);

class BoardSettingsValidator final : public config::ISettingsValidator {
public:
    explicit BoardSettingsValidator(BoardOptions options) : options_(options) {}
    core::Result validate(const DeviceSettings &settings) const override;

private:
    BoardOptions options_;
};

}  // namespace kc868a6::board
