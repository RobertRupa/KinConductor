#pragma once

#include "board/board_profile.h"

namespace kc868a6::board {

enum class ResourceKind { Gpio, Uart, I2cAddress };

struct ResourceClaim {
    ResourceKind kind;
    uint8_t number;
    const char *owner;
    PinUse use = PinUse::Input;
};

const char *resourceKindName(ResourceKind kind);

class ResourceManager {
public:
    static constexpr size_t capacity = 48;

    core::Result claim(ResourceClaim resource);
    core::List<ResourceClaim> claims() const { return {claims_.data(), size_}; }

private:
    std::array<ResourceClaim, capacity> claims_{};
    size_t size_ = 0;
};

}  // namespace kc868a6::board
