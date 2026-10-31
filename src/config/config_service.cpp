#include "config/config_service.h"

namespace kc868a6::config {

core::Result ConfigService::begin() {
    if (ready_) return {};
    DeviceSettings candidate = defaults_;
    core::Result result = store_.load(defaults_, candidate);
    if (!result.ok()) {
        status_ = result;
        return result;
    }
    normalizeSettings(candidate);
    result = validateSettings(candidate);
    if (result.ok() && validator_) result = validator_->validate(candidate);
    if (!result.ok()) {
        status_ = result;
        return result;
    }
    settings_ = candidate;
    ready_ = true;
    status_ = {};
    return {};
}

core::Result ConfigService::update(DeviceSettings candidate) {
    if (!ready_) return {core::ErrorCode::StorageUnavailable, "device_settings"};
    normalizeSettings(candidate);
    core::Result result = validateSettings(candidate);
    if (result.ok() && validator_) result = validator_->validate(candidate);
    if (!result.ok()) return result;
    if (equalSettings(candidate, settings_)) return {};
    result = store_.save(candidate);
    if (!result.ok()) {
        // A failed readback may follow a committed write; reload before accepting more edits.
        ready_ = false;
        status_ = result;
        return result;
    }
    settings_ = candidate;
    status_ = {};
    return {};
}

}  // namespace kc868a6::config
