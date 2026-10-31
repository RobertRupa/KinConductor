#pragma once

#include <string_view>

#ifndef KC868_TARGET_MODEL
#define KC868_TARGET_MODEL "KC868-A6"
#endif

namespace kc868a6::board {

inline constexpr std::string_view targetModel{KC868_TARGET_MODEL};
inline constexpr bool targetPinoutValidated = targetModel == "KC868-A6";

}  // namespace kc868a6::board
