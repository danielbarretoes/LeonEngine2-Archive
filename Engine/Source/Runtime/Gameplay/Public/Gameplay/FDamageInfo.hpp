#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Leon {

    class AActor;
    class UActorComponent;

    enum class EDamageType : uint8_t { Generic = 0, Point = 1, Radial = 2 };

    /**
     * @brief Generic hit / damage payload. Contains no game-mode rules.
     */
    struct FDamageInfo {
        float DamageAmount = 0.0f;
        EDamageType DamageType = EDamageType::Generic;
        AActor* Instigator = nullptr;
        AActor* Causer = nullptr;
        AActor* HitActor = nullptr;
        UActorComponent* HitComponent = nullptr;
        glm::vec3 HitLocation{0.0f};
        glm::vec3 HitNormal{0.0f, 1.0f, 0.0f};
        bool bCriticalHit = false;
    };

} // namespace Leon
