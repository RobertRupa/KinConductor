#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>
#include <optional>

namespace kc868a6::core {

enum class ErrorCode {
    None,
    InvalidArgument,
    NotFound,
    DuplicateId,
    CapacityExceeded,
    Unavailable,
    OperationFailed,
    StorageUnavailable,
    StorageWriteFailed,
    UnsupportedVersion,
    ResourceConflict,
};

struct Result {
    ErrorCode code = ErrorCode::None;
    std::string_view subject;

    bool ok() const { return code == ErrorCode::None; }
};

const char *errorCodeName(ErrorCode code);

template <typename T>
struct List {
    const T *data = nullptr;
    size_t size = 0;

    List() = default;
    List(const T *items, size_t count) : data(items), size(count) {}

    template <size_t N>
    List(const std::array<T, N> &items) : data(items.data()), size(N) {}

    template <size_t N>
    List(std::array<T, N> &&) = delete;
};

enum class ValueType { Boolean, SignedInteger, UnsignedInteger, Number, Text };
using Value = std::variant<bool, int32_t, uint32_t, double, std::string_view>;

bool valueMatchesType(const Value &value, ValueType type);

enum class Quality { Valid, Stale, Unavailable, Error, Disabled, WarmingUp };

struct PropertyDescriptor {
    std::string_view id;
    ValueType type;
    std::string_view unit;
    uint32_t staleAfterMs = 0;
};

struct PropertySnapshot {
    Value value = false;
    Quality quality = Quality::Unavailable;
    uint32_t updatedAtMs = 0;
    ErrorCode error = ErrorCode::None;
};

enum class TriggerKind { Condition, Event };

struct TriggerDescriptor {
    std::string_view id;
    TriggerKind kind;
    std::string_view propertyId;
};

struct ParameterDescriptor {
    std::string_view id;
    ValueType type;
    bool required = true;
    std::optional<double> minimum = std::nullopt;
    std::optional<double> maximum = std::nullopt;
};

bool valueMatchesParameter(const Value &value, const ParameterDescriptor &descriptor);

struct Parameter {
    std::string_view id;
    Value value;
};

struct ActionDescriptor {
    std::string_view id;
    List<ParameterDescriptor> parameters;
};

struct ActionResult {
    Result result;
    bool applied = false;
    bool confirmed = false;
};

struct PeripheralDescriptor {
    std::string_view id;
    std::string_view type;
};

struct Health {
    bool ready = false;
    ErrorCode error = ErrorCode::None;
};

// Descriptors are immutable and borrowed; owners outlive bindings and text-value reads.
class IPeripheral {
public:
    virtual ~IPeripheral() = default;
    virtual const PeripheralDescriptor &descriptor() const = 0;
    virtual Result begin(uint32_t nowMs) = 0;
    virtual Result tick(uint32_t nowMs) = 0;
    virtual void stop() = 0;
    virtual Health health() const = 0;
};

class IPropertyProvider {
public:
    virtual ~IPropertyProvider() = default;
    virtual List<PropertyDescriptor> properties() const = 0;
    virtual Result readProperty(std::string_view id,
                                PropertySnapshot &snapshot) const = 0;
};

class ITriggerProvider {
public:
    virtual ~ITriggerProvider() = default;
    virtual List<TriggerDescriptor> triggers() const = 0;
};

class IActionProvider {
public:
    virtual ~IActionProvider() = default;
    virtual List<ActionDescriptor> actions() const = 0;
    virtual Result validate(std::string_view, List<Parameter>) const { return {}; }
    virtual ActionResult execute(std::string_view id,
                                 List<Parameter> parameters) = 0;
};

}  // namespace kc868a6::core
