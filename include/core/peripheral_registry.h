#pragma once

#include "core/peripheral.h"

namespace kc868a6::core {

struct PeripheralBinding {
    IPeripheral *peripheral = nullptr;
    IPropertyProvider *properties = nullptr;
    ITriggerProvider *triggers = nullptr;
    IActionProvider *actions = nullptr;
};

class PeripheralRegistry {
public:
    static constexpr size_t capacity = 8;
    static constexpr size_t maxCapabilitiesPerKind = 16;
    static constexpr size_t maxParameters = 8;

    PeripheralRegistry() = default;
    PeripheralRegistry(const PeripheralRegistry &) = delete;
    PeripheralRegistry &operator=(const PeripheralRegistry &) = delete;

    Result add(PeripheralBinding binding);
    Result remove(std::string_view peripheralId);
    Result begin(uint32_t nowMs);
    Result tick(uint32_t nowMs);
    void stop();

    const PropertyDescriptor *findProperty(std::string_view id) const;
    const TriggerDescriptor *findTrigger(std::string_view id) const;
    const ActionDescriptor *findAction(std::string_view id) const;
    Result readProperty(std::string_view id, uint32_t nowMs,
                        PropertySnapshot &snapshot) const;
    ActionResult execute(std::string_view id, List<Parameter> parameters);
    Result validateAction(std::string_view id, List<Parameter> parameters) const;

    size_t size() const { return size_; }
    uint32_t revision() const { return revision_; }
    const PeripheralBinding *binding(size_t index) const {
        return index < size_ ? &entries_[index].binding : nullptr;
    }

private:
    struct Entry {
        PeripheralBinding binding;
        bool started = false;
    };

    std::array<Entry, capacity> entries_{};
    size_t size_ = 0;
    uint32_t revision_ = 0;
};

}  // namespace kc868a6::core
