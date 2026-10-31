#pragma once

#include "peripherals/ble_presence.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>

namespace kc868a6::peripherals {

// One application-lifetime instance owns NimBLE's shared scanner. The caller
// owns init/deinit and must stop this adapter before deinit. Never call
// getResults(duration) elsewhere while this adapter is enabled.
// Lifecycle methods run on the application task, never inside BLE callbacks.
class Esp32BlePresenceScanner final : public IBlePresenceScanner,
                                      private NimBLEScanCallbacks {
public:
    Esp32BlePresenceScanner() = default;
    Esp32BlePresenceScanner(const Esp32BlePresenceScanner &) = delete;
    Esp32BlePresenceScanner &operator=(const Esp32BlePresenceScanner &) = delete;
    core::Result start() override;
    void stop() override;
    void setTarget(const BleAddress *address) override;
    core::Result poll(BleScanSnapshot &snapshot) override;

private:
    void onDiscovered(const NimBLEAdvertisedDevice *device) override;
    void onResult(const NimBLEAdvertisedDevice *device) override;
    void observe(const NimBLEAdvertisedDevice *device);
    portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    NimBLEScan *scanner_ = nullptr;
    BleScanSnapshot cache_{};
    BleAddress target_{};
    bool hasTarget_ = false;
    bool accepting_ = false;
    uint32_t callbacksInFlight_ = 0;
};

}  // namespace kc868a6::peripherals
#endif
