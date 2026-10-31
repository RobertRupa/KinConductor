#include "board/resource_manager.h"

namespace kc868a6::board {

const char *resourceKindName(ResourceKind kind) {
    switch (kind) {
        case ResourceKind::Gpio: return "gpio";
        case ResourceKind::Uart: return "uart";
        case ResourceKind::I2cAddress: return "i2c_address";
    }
    return "invalid";
}

core::Result ResourceManager::claim(ResourceClaim resource) {
    if (resource.owner == nullptr || resource.owner[0] == '\0') {
        return {core::ErrorCode::InvalidArgument, "resource_owner"};
    }
    switch (resource.kind) {
        case ResourceKind::Gpio: {
            const auto *pin = findPin(resource.number);
            if (!pin || !supports(*pin, resource.use)) {
                return {core::ErrorCode::InvalidArgument, resource.owner};
            }
            break;
        }
        case ResourceKind::Uart:
            if (resource.number >= uartCount) {
                return {core::ErrorCode::InvalidArgument, resource.owner};
            }
            break;
        case ResourceKind::I2cAddress:
            if (resource.number < 0x08 || resource.number > 0x77) {
                return {core::ErrorCode::InvalidArgument, resource.owner};
            }
            break;
        default: return {core::ErrorCode::InvalidArgument, resource.owner};
    }
    for (size_t index = 0; index < size_; ++index) {
        if (claims_[index].kind == resource.kind &&
            claims_[index].number == resource.number) {
            return {core::ErrorCode::ResourceConflict, resource.owner};
        }
    }
    if (size_ == capacity) {
        return {core::ErrorCode::CapacityExceeded, "board_resources"};
    }
    claims_[size_++] = resource;
    return {};
}

}  // namespace kc868a6::board
