#pragma once

#include <cstdint>

namespace Leon {

    enum class EMovementMode : uint8_t {
        None = 0,
        Walking = 1,
        NavWalking = 2,
        Falling = 3,
        Swimming = 4,
        Flying = 5,
        Custom = 6,
    };

    inline constexpr EMovementMode MOVE_None = EMovementMode::None;
    inline constexpr EMovementMode MOVE_Walking = EMovementMode::Walking;
    inline constexpr EMovementMode MOVE_NavWalking = EMovementMode::NavWalking;
    inline constexpr EMovementMode MOVE_Falling = EMovementMode::Falling;
    inline constexpr EMovementMode MOVE_Swimming = EMovementMode::Swimming;
    inline constexpr EMovementMode MOVE_Flying = EMovementMode::Flying;
    inline constexpr EMovementMode MOVE_Custom = EMovementMode::Custom;

    inline const char* MovementModeName(EMovementMode InMode) {
        switch (InMode) {
        case EMovementMode::Walking:
            return "Walking";
        case EMovementMode::NavWalking:
            return "NavWalking";
        case EMovementMode::Falling:
            return "Falling";
        case EMovementMode::Swimming:
            return "Swimming";
        case EMovementMode::Flying:
            return "Flying";
        case EMovementMode::Custom:
            return "Custom";
        default:
            return "None";
        }
    }

} // namespace Leon
