#include "web/capabilities.h"

namespace kc868a6::web {
const char *valueTypeName(core::ValueType type) {
    switch (type) {
        case core::ValueType::Boolean: return "boolean";
        case core::ValueType::SignedInteger: return "integer";
        case core::ValueType::UnsignedInteger: return "unsigned";
        case core::ValueType::Number: return "number";
        case core::ValueType::Text: return "text";
    }
    return "invalid";
}
const char *qualityName(core::Quality quality) {
    switch (quality) {
        case core::Quality::Valid: return "valid";
        case core::Quality::Stale: return "stale";
        case core::Quality::Unavailable: return "unavailable";
        case core::Quality::Error: return "error";
        case core::Quality::Disabled: return "disabled";
        case core::Quality::WarmingUp: return "warming_up";
    }
    return "invalid";
}
core::Result writeCapabilities(JsonObject destination, const core::PeripheralRegistry &registry,
                               uint32_t nowMs) {
    bool ok = destination["schemaVersion"].set(1) &&
              destination["revision"].set(registry.revision());
    auto modules = destination.createNestedArray("modules");
    for (size_t index = 0; index < registry.size() && ok; ++index) {
        const auto &binding = *registry.binding(index);
        const auto &identity = binding.peripheral->descriptor();
        const auto health = binding.peripheral->health();
        auto module = modules.createNestedObject();
        ok = module["id"].set(identity.id) && module["type"].set(identity.type) &&
             module["ready"].set(health.ready) &&
             module["error"].set(core::errorCodeName(health.error));
        auto properties = module.createNestedArray("properties");
        const auto descriptors = binding.properties ? binding.properties->properties()
                                                    : core::List<core::PropertyDescriptor>{};
        for (size_t prop = 0; prop < descriptors.size && ok; ++prop) {
            const auto &descriptor = descriptors.data[prop];
            core::PropertySnapshot snapshot;
            const auto read = registry.readProperty(descriptor.id, nowMs, snapshot);
            if (!read.ok()) {
                snapshot.quality = core::Quality::Error;
                snapshot.error = read.code;
            }
            auto item = properties.createNestedObject();
            ok = item["id"].set(descriptor.id) &&
                 item["type"].set(valueTypeName(descriptor.type)) &&
                 (descriptor.unit.empty() ? item["unit"].set("") : item["unit"].set(descriptor.unit)) &&
                 item["quality"].set(qualityName(snapshot.quality)) &&
                 item["updatedAtMs"].set(snapshot.updatedAtMs) &&
                 item["error"].set(core::errorCodeName(snapshot.error));
            if (snapshot.quality == core::Quality::Valid) {
                ok = ok && std::visit([item](const auto &value) {
                    return item["value"].set(value);
                }, snapshot.value);
            } else {
                ok = ok && item["value"].set(nullptr);
            }
        }
        auto triggers = module.createNestedArray("triggers");
        const auto conditions = binding.triggers ? binding.triggers->triggers()
                                                : core::List<core::TriggerDescriptor>{};
        for (size_t trig = 0; trig < conditions.size && ok; ++trig) {
            const auto &descriptor = conditions.data[trig];
            auto item = triggers.createNestedObject();
            ok = item["id"].set(descriptor.id) &&
                 item["kind"].set(descriptor.kind == core::TriggerKind::Condition ? "condition" : "event") &&
                 (descriptor.propertyId.empty() ? item["propertyId"].set("") : item["propertyId"].set(descriptor.propertyId));
        }
        auto actions = module.createNestedArray("actions");
        const auto commands = binding.actions ? binding.actions->actions()
                                              : core::List<core::ActionDescriptor>{};
        for (size_t action = 0; action < commands.size && ok; ++action) {
            const auto &descriptor = commands.data[action];
            auto item = actions.createNestedObject();
            ok = item["id"].set(descriptor.id);
            auto parameters = item.createNestedArray("parameters");
            for (size_t param = 0; param < descriptor.parameters.size && ok; ++param) {
                const auto &argument = descriptor.parameters.data[param];
                auto field = parameters.createNestedObject();
                ok = field["id"].set(argument.id) &&
                     field["type"].set(valueTypeName(argument.type)) &&
                     field["required"].set(argument.required);
                if (argument.minimum) ok = ok && field["minimum"].set(*argument.minimum);
                if (argument.maximum) ok = ok && field["maximum"].set(*argument.maximum);
            }
            ok = ok && !parameters.isNull();
        }
        ok = ok && !properties.isNull() && !triggers.isNull() && !actions.isNull();
    }
    return ok && !modules.isNull() ? core::Result{}
        : core::Result{core::ErrorCode::CapacityExceeded, "capabilities_json"};
}

core::ActionResult executeJsonAction(core::PeripheralRegistry &registry, JsonVariantConst request,
                                    bool execute) {
    if (!request.is<JsonObjectConst>() || request.size() != 2 ||
        !request["id"].is<const char *>() || !request["parameters"].is<JsonObjectConst>()) {
        return {{core::ErrorCode::InvalidArgument, "action_request"}};
    }
    const JsonString actionId = request["id"].as<JsonString>();
    const std::string_view id(actionId.c_str(), actionId.size());
    const auto *descriptor = registry.findAction(id);
    if (!descriptor) return {{core::ErrorCode::NotFound, "action"}};
    std::array<core::Parameter, core::PeripheralRegistry::maxParameters> parameters{};
    const auto object = request["parameters"].as<JsonObjectConst>();
    if (object.size() > parameters.size()) return {{core::ErrorCode::CapacityExceeded, "parameters"}};
    size_t count = 0;
    for (JsonPairConst pair : object) {
        const std::string_view name(pair.key().c_str(), pair.key().size());
        const core::ParameterDescriptor *field = nullptr;
        for (size_t index = 0; index < descriptor->parameters.size; ++index) {
            if (descriptor->parameters.data[index].id == name) field = &descriptor->parameters.data[index];
        }
        if (!field) return {{core::ErrorCode::InvalidArgument, "parameter"}};
        auto &parameter = parameters[count++];
        parameter.id = name;
        const auto value = pair.value();
        switch (field->type) {
            case core::ValueType::Boolean:
                if (!value.is<bool>()) return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                parameter.value = value.as<bool>(); break;
            case core::ValueType::UnsignedInteger:
                if (!value.is<uint32_t>()) return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                parameter.value = value.as<uint32_t>(); break;
            case core::ValueType::SignedInteger:
                if (!value.is<int32_t>()) return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                parameter.value = value.as<int32_t>(); break;
            case core::ValueType::Number:
                if (!value.is<double>() || value.is<bool>()) return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                parameter.value = value.as<double>();
                if (!core::valueMatchesType(parameter.value, field->type)) {
                    return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                }
                break;
            case core::ValueType::Text:
                if (!value.is<const char *>()) return {{core::ErrorCode::InvalidArgument, "parameter_type"}};
                {
                    const JsonString text = value.as<JsonString>();
                    parameter.value = std::string_view(text.c_str(), text.size());
                }
                break;
        }
        if (!core::valueMatchesParameter(parameter.value, *field)) {
            return {{core::ErrorCode::InvalidArgument, "parameter_range"}};
        }
    }
    for (size_t index = 0; index < descriptor->parameters.size; ++index) {
        const auto &field = descriptor->parameters.data[index];
        if (field.required && !object.containsKey(field.id.data())) {
            return {{core::ErrorCode::InvalidArgument, "required_parameter"}};
        }
    }
    const auto valid = registry.validateAction(id, {parameters.data(), count});
    if (!valid.ok()) return {{valid.code, "action_parameters"}, false, false};
    if (!execute) return {{}, false, false};
    auto result = registry.execute(id, {parameters.data(), count});
    // Do not return borrowed request strings beyond the document's lifetime.
    if (!result.result.ok()) result.result.subject = "action";
    return result;
}
}
