#include "peripherals/esp32_ble_presence_scanner.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <freertos/task.h>

namespace kc868a6::peripherals {

core::Result Esp32BlePresenceScanner::start() {
    if (!NimBLEDevice::isInitialized()) {
        return {core::ErrorCode::Unavailable, "ble_stack"};
    }
    if (scanner_) return {core::ErrorCode::ResourceConflict, "ble_scanner"};
    NimBLEScan *scanner = NimBLEDevice::getScan();
    if (!scanner || scanner->isScanning()) {
        return {core::ErrorCode::ResourceConflict, "ble_scanner"};
    }
    scanner_ = scanner;
    scanner_->clearResults();
    // Zero stores no NimBLE results. Duplicate callbacks are essential: a
    // continuously present target must refresh its timeout on every sighting.
    scanner_->setMaxResults(0);
    scanner_->setScanCallbacks(this, true);
    scanner_->setDuplicateFilter(0);
    scanner_->setActiveScan(true);
    scanner_->setInterval(160);
    scanner_->setWindow(80);
    portENTER_CRITICAL(&mutex_);
    cache_ = {};
    accepting_ = true;
    portEXIT_CRITICAL(&mutex_);
    if (!scanner_->start(0, false, false)) {
        stop();
        return {core::ErrorCode::OperationFailed, "ble_scan_start"};
    }
    return {};
}

void Esp32BlePresenceScanner::stop() {
    portENTER_CRITICAL(&mutex_);
    accepting_ = false;
    cache_ = {};
    portEXIT_CRITICAL(&mutex_);
    // An advertisement object belongs to NimBLE. Let callbacks already
    // copying it finish before cancellation can delete its scan result.
    for (;;) {
        portENTER_CRITICAL(&mutex_);
        const bool drained = callbacksInFlight_ == 0;
        portEXIT_CRITICAL(&mutex_);
        if (drained) break;
        vTaskDelay(1);
    }
    if (scanner_ && NimBLEDevice::isInitialized()) {
        scanner_->stop();
        scanner_->setScanCallbacks(nullptr);
        scanner_->clearResults();
    }
    scanner_ = nullptr;
}

void Esp32BlePresenceScanner::setTargets(const BleTargets &targets) {
    portENTER_CRITICAL(&mutex_);
    targets_ = targets;
    cache_.targetObserved = {};
    cache_.targetSeenAtMs = {};
    portEXIT_CRITICAL(&mutex_);
}

core::Result Esp32BlePresenceScanner::poll(BleScanSnapshot &snapshot) {
    if (!scanner_ || !NimBLEDevice::isInitialized() || !scanner_->isScanning()) {
        snapshot = {};
        return {core::ErrorCode::Unavailable, "ble_scanner"};
    }
    portENTER_CRITICAL(&mutex_);
    snapshot = cache_;
    cache_.targetObserved = {};
    portEXIT_CRITICAL(&mutex_);
    return {};
}

void Esp32BlePresenceScanner::onDiscovered(const NimBLEAdvertisedDevice *device) {
    observe(device);
}

void Esp32BlePresenceScanner::onResult(const NimBLEAdvertisedDevice *device) {
    observe(device);
}

void Esp32BlePresenceScanner::observe(const NimBLEAdvertisedDevice *device) {
    if (!device) return;
    portENTER_CRITICAL(&mutex_);
    if (!accepting_) {
        portEXIT_CRITICAL(&mutex_);
        return;
    }
    ++callbacksInFlight_;
    portEXIT_CRITICAL(&mutex_);
    BleScanDevice entry;
    const auto address = device->getAddress();
    // NimBLE's native address is little-endian; core/UI addresses are printed order.
    for (size_t i = 0; i < entry.address.size(); ++i) {
        entry.address[i] = address.getVal()[5 - i];
    }
    entry.addressType = address.getType();
    entry.rssi = device->getRSSI();
    entry.seenAtMs = millis();
    const std::string name = device->getName();
    const size_t length = std::min(name.size(), entry.name.size() - 1);
    std::memcpy(entry.name.data(), name.data(), length);

    portENTER_CRITICAL(&mutex_);
    if (accepting_) {
        for (size_t i = 0; i < targets_.count; ++i) if (entry.address == targets_.addresses[i]) {
            cache_.targetObserved[i] = true;
            cache_.targetSeenAtMs[i] = entry.seenAtMs;
        }
        size_t index = 0;
        while (index < cache_.count &&
               !(cache_.devices[index].address == entry.address &&
                 cache_.devices[index].addressType == entry.addressType)) ++index;
        if (index == cache_.count) {
            if (cache_.count < BleScanSnapshot::capacity) {
                ++cache_.count;
            } else {
                index = 0;
                for (size_t i = 1; i < cache_.count; ++i) {
                    if (static_cast<uint32_t>(entry.seenAtMs - cache_.devices[i].seenAtMs) >
                        static_cast<uint32_t>(entry.seenAtMs - cache_.devices[index].seenAtMs)) {
                        index = i;
                    }
                }
            }
        }
        if (entry.name[0] == '\0' && cache_.devices[index].address == entry.address) {
            entry.name = cache_.devices[index].name;
        }
        cache_.devices[index] = entry;
    }
    --callbacksInFlight_;
    portEXIT_CRITICAL(&mutex_);
}

}  // namespace kc868a6::peripherals
#endif
