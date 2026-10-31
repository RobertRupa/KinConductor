#pragma once

#include <Preferences.h>

#include "config/config_service.h"

namespace kc868a6::config {

class PreferencesSettingsStore final : public ISettingsStore {
public:
    core::Result load(const DeviceSettings &defaults,
                      DeviceSettings &settings) override;
    core::Result save(const DeviceSettings &settings) override;

private:
    Preferences preferences_;
    bool opened_ = false;

    core::Result open();
};

}  // namespace kc868a6::config
