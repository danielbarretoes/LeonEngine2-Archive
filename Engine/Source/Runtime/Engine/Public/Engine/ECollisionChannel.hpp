#pragma once

#include <cstdint>

namespace Leon {

    /**
     * Generic collision / trace channels. Games pick which channel a query uses;
     * the engine does not know about weapons or teams.
     */
    enum class ECollisionChannel : uint8_t {
        Visibility = 0,
        WorldStatic = 1,
        WorldDynamic = 2,
        Pawn = 3,
        Camera = 4,
        PhysicsBody = 5,
        GameTraceChannel1 = 6,
        GameTraceChannel2 = 7,
        GameTraceChannel3 = 8,
    };

    inline bool TraceChannelAccepts(ECollisionChannel InQuery, ECollisionChannel InCollider) {
        if (InQuery == ECollisionChannel::Visibility)
            return InCollider == ECollisionChannel::WorldStatic || InCollider == ECollisionChannel::WorldDynamic ||
                   InCollider == ECollisionChannel::Pawn || InCollider == ECollisionChannel::PhysicsBody;
        if (InQuery == ECollisionChannel::Camera)
            return InCollider == ECollisionChannel::WorldStatic || InCollider == ECollisionChannel::WorldDynamic;
        if (InQuery == ECollisionChannel::Pawn)
            return InCollider == ECollisionChannel::WorldStatic || InCollider == ECollisionChannel::WorldDynamic ||
                   InCollider == ECollisionChannel::Pawn || InCollider == ECollisionChannel::PhysicsBody;
        return InQuery == InCollider;
    }

} // namespace Leon
