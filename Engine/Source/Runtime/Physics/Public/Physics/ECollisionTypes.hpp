#pragma once

#include "Engine/ECollisionChannel.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace Leon {

    enum class ECollisionEnabled : uint8_t {
        NoCollision = 0,
        QueryOnly = 1,
        PhysicsOnly = 2,
        QueryAndPhysics = 3,
    };

    enum class ECollisionResponse : uint8_t { Ignore = 0, Overlap = 1, Block = 2 };

    enum class EPhysicsMotionType : uint8_t { Static = 0, Kinematic = 1, Dynamic = 2 };

    enum class EPhysicsShapeType : uint8_t { Box = 0, Sphere = 1, Capsule = 2 };

    constexpr uint8_t kCollisionChannelCount = 16;

    struct FCollisionResponseContainer {
        std::array<ECollisionResponse, kCollisionChannelCount> Channels{};

        FCollisionResponseContainer() { Channels.fill(ECollisionResponse::Block); }

        ECollisionResponse Get(ECollisionChannel InChannel) const {
            const uint8_t i = static_cast<uint8_t>(InChannel);
            return i < kCollisionChannelCount ? Channels[i] : ECollisionResponse::Block;
        }

        void Set(ECollisionChannel InChannel, ECollisionResponse InResponse) {
            const uint8_t i = static_cast<uint8_t>(InChannel);
            if (i < kCollisionChannelCount)
                Channels[i] = InResponse;
        }
    };

    inline const char* CollisionChannelName(ECollisionChannel InChannel) {
        switch (InChannel) {
        case ECollisionChannel::Visibility:
            return "Visibility";
        case ECollisionChannel::WorldStatic:
            return "WorldStatic";
        case ECollisionChannel::WorldDynamic:
            return "WorldDynamic";
        case ECollisionChannel::Pawn:
            return "Pawn";
        case ECollisionChannel::Camera:
            return "Camera";
        case ECollisionChannel::PhysicsBody:
            return "PhysicsBody";
        default:
            return "Game";
        }
    }

} // namespace Leon
