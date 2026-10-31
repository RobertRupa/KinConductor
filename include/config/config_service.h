#pragma once

#include "config/device_settings.h"

namespace kc868a6::config {

class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;
    virtual core::Result load(const DeviceSettings &defaults,
                              DeviceSettings &settings) = 0;
    virtual core::Result save(const DeviceSettings &settings) = 0;
};

class ISettingsValidator {
public:
    virtual ~ISettingsValidator() = default;
    virtual core::Result validate(const DeviceSettings &settings) const = 0;
};

class ConfigService {
public:
    ConfigService(ISettingsStore &store, DeviceSettings defaults,
                  const ISettingsValidator *validator = nullptr)
        : store_(store), defaults_(defaults), settings_(defaults), validator_(validator) {}

    core::Result begin();
    core::Result update(DeviceSettings candidate);
    const DeviceSettings &settings() const { return settings_; }
    bool ready() const { return ready_; }
    core::Result status() const { return status_; }

private:
    ISettingsStore &store_;
    DeviceSettings defaults_;
    DeviceSettings settings_;
    const ISettingsValidator *validator_;
    bool ready_ = false;
    core::Result status_{core::ErrorCode::StorageUnavailable, "device_settings"};
};

}  // namespace kc868a6::config
