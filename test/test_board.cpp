#include "board/settings_resources.h"
#include "config/preferences_settings_store.h"
#include "web/board_status.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace kc868a6;

namespace {

void testPinProfile() {
    const auto pins = board::pins();
    assert(pins.size == 26 && board::uartCount == 3);
    for (size_t index = 0; index < pins.size; ++index) {
        assert(board::findPin(pins.data[index].number) == &pins.data[index]);
        for (size_t previous = 0; previous < index; ++previous) {
            assert(pins.data[index].number != pins.data[previous].number);
        }
    }
    for (uint8_t pin : gpio::analogInputs) {
        const auto *descriptor = board::findPin(pin);
        assert(descriptor && descriptor->inputOnly && descriptor->adc);
        assert(!board::supports(*descriptor, board::PinUse::Output));
        assert(!board::supports(*descriptor, board::PinUse::AnalogOutput));
    }
    assert(board::findPin(12)->strapping);
    assert(board::p17HostTx == 12 && board::p17HostRx == 13);
    assert(gpio::ttlRx == 12 && gpio::ttlTx == 13);
    assert(gpio::pn532SpiCs == board::p17HostRx);
    assert(board::relayPortMask == 0x3F && board::relayReservedPortMask == 0xC0);
    assert(board::supports(*board::findPin(26), board::PinUse::AnalogOutput));
    assert(!board::supports(*board::findPin(32), board::PinUse::AnalogOutput));
    assert(board::findPin(6) == nullptr);
}

void testClaimsAndBounds() {
    board::ResourceManager manager;
    assert(manager.claim({board::ResourceKind::Gpio, 34, "output",
                          board::PinUse::Output}).code == core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::Gpio, 6, "flash"}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::Uart, 3, "fourth_uart"}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::I2cAddress, 0x78, "reserved"}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::I2cAddress, 0, "general_call"}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::Gpio, 12, nullptr}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({static_cast<board::ResourceKind>(99), 12, "invalid"}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claim({board::ResourceKind::Gpio, 34, "invalid_use",
                          static_cast<board::PinUse>(99)}).code ==
           core::ErrorCode::InvalidArgument);
    assert(manager.claims().size == 0);
    assert(manager.claim({board::ResourceKind::Gpio, 12, "first",
                          board::PinUse::Output}).ok());
    const auto result = manager.claim({board::ResourceKind::Gpio, 12, "second"});
    assert(result.code == core::ErrorCode::ResourceConflict && result.subject == "second");
    assert(manager.claims().size == 1 && manager.claims().data[0].number == 12);
    assert(manager.claim({board::ResourceKind::I2cAddress, 12, "different_kind"}).ok());
    assert(manager.claims().size == 2);
    assert(std::string(core::errorCodeName(result.code)) == "resource_conflict");
}

void testCapacityAndDuplicateClaims() {
    board::ResourceManager manager;
    for (size_t index = 0; index < board::ResourceManager::capacity; ++index) {
        assert(manager.claim({board::ResourceKind::I2cAddress,
                              static_cast<uint8_t>(0x08 + index), "i2c_device"}).ok());
    }
    assert(manager.claim({board::ResourceKind::I2cAddress, 0x70, "overflow"}).code ==
           core::ErrorCode::CapacityExceeded);
    assert(manager.claim({board::ResourceKind::I2cAddress, 0x24, "same_address"}).code ==
           core::ErrorCode::ResourceConflict);
    assert(manager.claims().size == board::ResourceManager::capacity);
}

void testExistingProfilesAndSharedBuses() {
    const board::Radio radios[] = {board::Radio::None, board::Radio::LoRa, board::Radio::Nrf24};
    const config::Uart1Mode modes[] = {
        config::Uart1Mode::Serial, config::Uart1Mode::Gps, config::Uart1Mode::Coprocessor
    };
    for (auto radio : radios) {
        for (auto mode : modes) {
            board::BoardOptions options;
            options.radio = radio;
            board::ResourceManager manager;
            assert(board::planResources(config::defaultSettings(mode), options, manager).ok());
            assert(manager.claims().size < board::ResourceManager::capacity);
            size_t sckClaims = 0;
            size_t uartClaims = 0;
            size_t pn532Claims = 0;
            for (size_t index = 0; index < manager.claims().size; ++index) {
                const auto &claim = manager.claims().data[index];
                if (claim.kind == board::ResourceKind::Gpio && claim.number == gpio::spiSck) {
                    ++sckClaims;
                }
                if (claim.kind == board::ResourceKind::Uart) ++uartClaims;
                if (std::string_view(claim.owner) == "pn532") ++pn532Claims;
            }
            assert(sckClaims == 1 && uartClaims == 3 && pn532Claims == 1);
        }
    }
}

void testBoardConflictsAndAtomicPlan() {
    DeviceSettings settings;
    board::BoardOptions options;
    board::ResourceManager manager;
    assert(manager.claim({board::ResourceKind::Gpio, 25, "previous"}).ok());
    options.p17 = board::P17Uart::Uart1;
    assert(board::planResources(settings, options, manager).code ==
           core::ErrorCode::ResourceConflict);
    assert(manager.claims().size == 1 &&
           std::string_view(manager.claims().data[0].owner) == "previous");
    options.rs232 = false;
    const auto pnConflict = board::planResources(settings, options, manager);
    assert(pnConflict.code == core::ErrorCode::ResourceConflict && pnConflict.subject == "pn532");
    settings.pn532Enabled = false;
    assert(board::planResources(settings, options, manager).ok());
    options.p17 = board::P17Uart::Uart2;
    assert(board::planResources(settings, options, manager).code ==
           core::ErrorCode::ResourceConflict);
    options.rs485 = false;
    options.rs232 = true;
    assert(board::planResources(settings, options, manager).ok());
    options = {};
    options.nfc = board::NfcTransport::I2c;
    settings.pn532Enabled = true;
    assert(board::planResources(settings, options, manager).code ==
           core::ErrorCode::ResourceConflict);
    options = {};
    options.dac1 = true;
    settings.beeperEnabled = true;
    const auto dacConflict = board::planResources(settings, options, manager);
    assert(dacConflict.code == core::ErrorCode::ResourceConflict && dacConflict.subject == "dac1");
    settings.beeperEnabled = false;
    assert(board::planResources(settings, options, manager).ok());
    options = {};
    settings.beeperEnabled = true;
    settings.fanEnabled = true;
    assert(board::planResources(settings, options, manager).ok());
    settings.fanPin = settings.beeperPin;
    assert(board::planResources(settings, options, manager).code == core::ErrorCode::ResourceConflict);
    settings.beeperPin = 25;
    settings.fanPin = 26;
    assert(board::planResources(settings, options, manager).ok());
}

void testConfigValidationBeforeStorage() {
    Preferences::reset();
    config::PreferencesSettingsStore store;
    board::BoardOptions options;
    options.dac1 = true;
    board::BoardSettingsValidator validator{options};
    config::ConfigService service{store, DeviceSettings{}, &validator};
    assert(service.begin().ok());
    DeviceSettings candidate = service.settings();
    candidate.beeperEnabled = true;
    assert(service.update(candidate).code == core::ErrorCode::ResourceConflict);
    assert(service.ready() && !service.settings().beeperEnabled && Preferences::writes == 0);
    assert(service.update(service.settings()).ok() && Preferences::writes == 0);
    assert(store.save(candidate).ok());
    config::ConfigService restarted{store, DeviceSettings{}, &validator};
    assert(restarted.begin().code == core::ErrorCode::ResourceConflict);
    assert(!restarted.ready() && !restarted.settings().beeperEnabled);
    assert(Preferences::writes == 1);
}

void testDiagnosticJson() {
    DynamicJsonDocument document(web::boardJsonCapacity);
    board::BoardOptions options;
    options.radio = board::Radio::LoRa;
    assert(web::writeBoardStatus(document.to<JsonObject>(), DeviceSettings{}, options, {}).ok());
    assert(!document.overflowed());
    assert(document["board"] == "KC868-A6");
    assert(document["pins"].size() == 26 && document["claims"].size() > 20);
    assert(document["valid"].as<bool>() && document["configurationReady"].as<bool>());
    assert(document["uartCount"] == 3 && document["relays"]["reservedMask"] == 0xC0);
    assert(document["pins"][6]["gpio"] == 12 && document["pins"][6]["strapping"].as<bool>());
    assert(document["pins"][22]["gpio"] == 34 && document["pins"][22]["inputOnly"].as<bool>());
    std::string json;
    serializeJson(document, json);
    assert(json.size() < web::boardJsonCapacity);
    assert(document.memoryUsage() < web::boardJsonCapacity);
    assert(document["board"] == "KC868-A6");
    assert(document["pinoutValidated"].as<bool>());
    document.clear();
    options.nfc = board::NfcTransport::I2c;
    assert(web::writeBoardStatus(document.to<JsonObject>(), DeviceSettings{}, options,
                                {core::ErrorCode::UnsupportedVersion, "settings"}).ok());
    assert(!document["valid"].as<bool>() && !document["configurationReady"].as<bool>());
    assert(document["error"] == "resource_conflict" && document["subject"] == "pn532");
    assert(document["claims"].size() == 0 && document["pins"].size() == 26);
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> tiny;
    assert(web::writeBoardStatus(tiny.to<JsonObject>(), DeviceSettings{}, {}, {}).code ==
           core::ErrorCode::CapacityExceeded);
}

}  // namespace

void runBoardTests() {
    testPinProfile();
    testClaimsAndBounds();
    testCapacityAndDuplicateClaims();
    testExistingProfilesAndSharedBuses();
    testBoardConflictsAndAtomicPlan();
    testConfigValidationBeforeStorage();
    testDiagnosticJson();
    std::cout << "PASS: 7 board test groups (pinout, resources, config validation, diagnostics)\n";
}
