#include "core/peripheral_registry.h"

#include <cmath>

namespace kc868a6::core {
namespace {

template <typename T>
Result validateList(List<T> list, size_t limit) {
    if ((list.size != 0 && list.data == nullptr) || list.size > limit) {
        return {ErrorCode::InvalidArgument, "descriptor_list"};
    }
    for (size_t index = 0; index < list.size; ++index) {
        if (list.data[index].id.empty()) {
            return {ErrorCode::InvalidArgument, "empty_id"};
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (list.data[previous].id == list.data[index].id) {
                return {ErrorCode::DuplicateId, list.data[index].id};
            }
        }
    }
    return {};
}

template <typename T>
const T *find(List<T> list, std::string_view id) {
    for (size_t index = 0; index < list.size; ++index) {
        if (list.data[index].id == id) return &list.data[index];
    }
    return nullptr;
}

Result validateParameters(List<ParameterDescriptor> descriptors,
                          List<Parameter> parameters) {
    if ((parameters.size != 0 && parameters.data == nullptr) ||
        parameters.size > PeripheralRegistry::maxParameters) {
        return {ErrorCode::InvalidArgument, "parameters"};
    }
    for (size_t index = 0; index < parameters.size; ++index) {
        const Parameter &parameter = parameters.data[index];
        const ParameterDescriptor *descriptor = find(descriptors, parameter.id);
        if (descriptor == nullptr ||
            !valueMatchesParameter(parameter.value, *descriptor)) {
            return {ErrorCode::InvalidArgument, parameter.id};
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (parameters.data[previous].id == parameter.id) {
                return {ErrorCode::DuplicateId, parameter.id};
            }
        }
    }
    for (size_t index = 0; index < descriptors.size; ++index) {
        if (descriptors.data[index].required &&
            find(parameters, descriptors.data[index].id) == nullptr) {
            return {ErrorCode::InvalidArgument, descriptors.data[index].id};
        }
    }
    return {};
}

}  // namespace

const char *errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::None: return "none";
        case ErrorCode::InvalidArgument: return "invalid_argument";
        case ErrorCode::NotFound: return "not_found";
        case ErrorCode::DuplicateId: return "duplicate_id";
        case ErrorCode::CapacityExceeded: return "capacity_exceeded";
        case ErrorCode::Unavailable: return "unavailable";
        case ErrorCode::OperationFailed: return "operation_failed";
        case ErrorCode::StorageUnavailable: return "storage_unavailable";
        case ErrorCode::StorageWriteFailed: return "storage_write_failed";
        case ErrorCode::UnsupportedVersion: return "unsupported_version";
        case ErrorCode::ResourceConflict: return "resource_conflict";
    }
    return "unknown_error";
}

bool valueMatchesParameter(const Value &value, const ParameterDescriptor &descriptor) {
    if (!valueMatchesType(value, descriptor.type)) return false;
    if (!descriptor.minimum && !descriptor.maximum) return true;
    return std::visit([&descriptor](const auto &item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
            const double number = static_cast<double>(item);
            return (!descriptor.minimum || number >= *descriptor.minimum) &&
                   (!descriptor.maximum || number <= *descriptor.maximum);
        }
        return false;
    }, value);
}

bool valueMatchesType(const Value &value, ValueType type) {
    switch (type) {
        case ValueType::Boolean: return std::holds_alternative<bool>(value);
        case ValueType::SignedInteger: return std::holds_alternative<int32_t>(value);
        case ValueType::UnsignedInteger: return std::holds_alternative<uint32_t>(value);
        case ValueType::Number: {
            const double *number = std::get_if<double>(&value);
            return number != nullptr && std::isfinite(*number);
        }
        case ValueType::Text: return std::holds_alternative<std::string_view>(value);
    }
    return false;
}

Result PeripheralRegistry::add(PeripheralBinding binding) {
    if (binding.peripheral == nullptr ||
        binding.peripheral->descriptor().id.empty() ||
        binding.peripheral->descriptor().type.empty()) {
        return {ErrorCode::InvalidArgument, "peripheral"};
    }
    const std::string_view id = binding.peripheral->descriptor().id;
    for (size_t index = 0; index < size_; ++index) {
        if (entries_[index].binding.peripheral->descriptor().id == id) {
            return {ErrorCode::DuplicateId, id};
        }
    }
    if (size_ == capacity) return {ErrorCode::CapacityExceeded, id};

    const auto properties = binding.properties ? binding.properties->properties()
                                              : List<PropertyDescriptor>{};
    const auto triggers = binding.triggers ? binding.triggers->triggers()
                                          : List<TriggerDescriptor>{};
    const auto actions = binding.actions ? binding.actions->actions()
                                        : List<ActionDescriptor>{};
    Result result = validateList(properties, maxCapabilitiesPerKind);
    if (!result.ok()) return result;
    result = validateList(triggers, maxCapabilitiesPerKind);
    if (!result.ok()) return result;
    result = validateList(actions, maxCapabilitiesPerKind);
    if (!result.ok()) return result;

    for (size_t index = 0; index < properties.size; ++index) {
        const std::string_view propertyId = properties.data[index].id;
        if (findProperty(propertyId) || findTrigger(propertyId) ||
            findAction(propertyId) || find(triggers, propertyId) ||
            find(actions, propertyId)) {
            return {ErrorCode::DuplicateId, propertyId};
        }
    }
    for (size_t index = 0; index < triggers.size; ++index) {
        const TriggerDescriptor &trigger = triggers.data[index];
        if (findProperty(trigger.id) || findTrigger(trigger.id) ||
            findAction(trigger.id) || find(actions, trigger.id)) {
            return {ErrorCode::DuplicateId, trigger.id};
        }
        if ((trigger.kind == TriggerKind::Condition &&
             find(properties, trigger.propertyId) == nullptr) ||
            (trigger.kind == TriggerKind::Event && !trigger.propertyId.empty())) {
            return {ErrorCode::InvalidArgument, trigger.id};
        }
    }
    for (size_t index = 0; index < actions.size; ++index) {
        const ActionDescriptor &action = actions.data[index];
        if (findProperty(action.id) || findTrigger(action.id) || findAction(action.id)) {
            return {ErrorCode::DuplicateId, action.id};
        }
        result = validateList(action.parameters, maxParameters);
        if (!result.ok()) return result;
    }
    entries_[size_++] = {binding, false};
    ++revision_;
    return {};
}

Result PeripheralRegistry::remove(std::string_view peripheralId) {
    for (size_t index = 0; index < size_; ++index) {
        Entry &entry = entries_[index];
        if (entry.binding.peripheral->descriptor().id != peripheralId) continue;
        if (entry.started) entry.binding.peripheral->stop();
        for (size_t next = index + 1; next < size_; ++next) {
            entries_[next - 1] = entries_[next];
        }
        entries_[--size_] = {};
        ++revision_;
        return {};
    }
    return {ErrorCode::NotFound, peripheralId};
}

Result PeripheralRegistry::begin(uint32_t nowMs) {
    Result firstError;
    for (size_t index = 0; index < size_; ++index) {
        Entry &entry = entries_[index];
        if (entry.started) continue;
        const Result result = entry.binding.peripheral->begin(nowMs);
        entry.started = result.ok();
        if (!result.ok() && firstError.ok()) firstError = result;
    }
    return firstError;
}

Result PeripheralRegistry::tick(uint32_t nowMs) {
    Result firstError;
    for (size_t index = 0; index < size_; ++index) {
        Entry &entry = entries_[index];
        if (!entry.started) continue;
        const Result result = entry.binding.peripheral->tick(nowMs);
        if (!result.ok() && firstError.ok()) firstError = result;
    }
    return firstError;
}

void PeripheralRegistry::stop() {
    for (size_t index = 0; index < size_; ++index) {
        Entry &entry = entries_[index];
        if (entry.started) entry.binding.peripheral->stop();
        entry.started = false;
    }
}

const PropertyDescriptor *PeripheralRegistry::findProperty(std::string_view id) const {
    for (size_t index = 0; index < size_; ++index) {
        const auto *provider = entries_[index].binding.properties;
        if (provider != nullptr) {
            if (const auto *descriptor = find(provider->properties(), id)) return descriptor;
        }
    }
    return nullptr;
}

const TriggerDescriptor *PeripheralRegistry::findTrigger(std::string_view id) const {
    for (size_t index = 0; index < size_; ++index) {
        const auto *provider = entries_[index].binding.triggers;
        if (provider != nullptr) {
            if (const auto *descriptor = find(provider->triggers(), id)) return descriptor;
        }
    }
    return nullptr;
}

const ActionDescriptor *PeripheralRegistry::findAction(std::string_view id) const {
    for (size_t index = 0; index < size_; ++index) {
        const auto *provider = entries_[index].binding.actions;
        if (provider != nullptr) {
            if (const auto *descriptor = find(provider->actions(), id)) return descriptor;
        }
    }
    return nullptr;
}

Result PeripheralRegistry::readProperty(std::string_view id, uint32_t nowMs,
                                       PropertySnapshot &snapshot) const {
    snapshot = {};
    for (size_t index = 0; index < size_; ++index) {
        const Entry &entry = entries_[index];
        const auto *provider = entry.binding.properties;
        const auto *descriptor = provider ? find(provider->properties(), id) : nullptr;
        if (descriptor == nullptr) continue;
        if (!entry.started) {
            snapshot.error = ErrorCode::Unavailable;
            return {ErrorCode::Unavailable, id};
        }
        const Result result = provider->readProperty(id, snapshot);
        if (!result.ok()) {
            snapshot = {};
            snapshot.quality = Quality::Error;
            snapshot.error = result.code;
            return result;
        }
        if (snapshot.quality == Quality::Valid &&
            !valueMatchesType(snapshot.value, descriptor->type)) {
            snapshot = {};
            snapshot.quality = Quality::Error;
            snapshot.error = ErrorCode::InvalidArgument;
            return {ErrorCode::InvalidArgument, id};
        }
        if (snapshot.quality == Quality::Valid && descriptor->staleAfterMs != 0 &&
            nowMs - snapshot.updatedAtMs > descriptor->staleAfterMs) {
            snapshot.quality = Quality::Stale;
        }
        return {};
    }
    snapshot.error = ErrorCode::NotFound;
    return {ErrorCode::NotFound, id};
}

ActionResult PeripheralRegistry::execute(std::string_view id,
                                        List<Parameter> parameters) {
    for (size_t index = 0; index < size_; ++index) {
        Entry &entry = entries_[index];
        auto *provider = entry.binding.actions;
        const auto *descriptor = provider ? find(provider->actions(), id) : nullptr;
        if (descriptor == nullptr) continue;
        if (!entry.started || !entry.binding.peripheral->health().ready) {
            return {{ErrorCode::Unavailable, id}, false, false};
        }
        const Result validation = validateAction(id, parameters);
        if (!validation.ok()) return {validation, false, false};
        return provider->execute(id, parameters);
    }
    return {{ErrorCode::NotFound, id}, false, false};
}

Result PeripheralRegistry::validateAction(std::string_view id, List<Parameter> parameters) const {
    for (size_t index = 0; index < size_; ++index) {
        const auto *provider = entries_[index].binding.actions;
        const auto *descriptor = provider ? find(provider->actions(), id) : nullptr;
        if (!descriptor) continue;
        const auto typed = validateParameters(descriptor->parameters, parameters);
        return typed.ok() ? provider->validate(id, parameters) : typed;
    }
    return {ErrorCode::NotFound, "action"};
}

}  // namespace kc868a6::core
