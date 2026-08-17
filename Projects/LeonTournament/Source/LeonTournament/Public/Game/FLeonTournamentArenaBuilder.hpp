#pragma once

#include "Engine/EMobility.hpp"

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Leon {

    class UWorld;
    class AActor;

    enum class ELeonTournamentArenaSurface : uint8_t {
        Floor = 0,
        Wall = 1,
        Prop = 2,
        Metal = 3,
        Accent = 4,
        Ceiling = 5
    };

    /** Shared procedural arena / lab geometry spawning (collision + PBR mesh). */
    struct FLeonTournamentArenaBuilder {
        static AActor* SpawnBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                const glm::vec3& InScale, ELeonTournamentArenaSurface InSurface,
                                const glm::vec3& InTint = glm::vec3(1.0f), float InUvTile = 1.0f);

        /** Lab-style cube using default material albedo only (no arena surface materials). */
        static AActor* SpawnSimpleBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                      const glm::vec3& InScale, const glm::vec3& InColor);

        static AActor* SpawnPointLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                       const glm::vec3& InColor, float InIntensity, float InRadius,
                                       ELightMobility InMobility = ELightMobility::Stationary);

        static AActor* SpawnSpotLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                      const glm::vec3& InDir, const glm::vec3& InColor, float InIntensity,
                                      float InRadius, float InInnerDeg, float InOuterDeg,
                                      ELightMobility InMobility = ELightMobility::Stationary);
    };

} // namespace Leon
