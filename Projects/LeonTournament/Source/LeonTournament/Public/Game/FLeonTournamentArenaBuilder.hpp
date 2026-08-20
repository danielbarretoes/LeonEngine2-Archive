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
        Ceiling = 5,
        Mirror = 6
    };

    /**
     * Procedural TDM arena: world-meter UVs, surface materials, Static bake lights.
     * Authored .lmap files are produced by Scripts/rebuild_tournament_arena.py (or BuildArena fallback).
     */
    struct FLeonTournamentArenaBuilder {
        static constexpr float kHalfExtent = 32.0f;
        static constexpr float kWallHeight = 10.0f;
        static constexpr float kCeilingY = 10.25f;

        static AActor* SpawnBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                const glm::vec3& InScale, ELeonTournamentArenaSurface InSurface,
                                const glm::vec3& InTint = glm::vec3(1.0f), float InMetersPerUv = 0.0f);

        static AActor* SpawnSimpleBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                      const glm::vec3& InScale, const glm::vec3& InColor);

        static AActor* SpawnPointLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                       const glm::vec3& InColor, float InIntensity, float InRadius,
                                       ELightMobility InMobility = ELightMobility::Static);

        static AActor* SpawnSpotLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                      const glm::vec3& InDir, const glm::vec3& InColor, float InIntensity,
                                      float InRadius, float InInnerDeg, float InOuterDeg,
                                      ELightMobility InMobility = ELightMobility::Static);

        /** Shell + interior cover + upper deck (day) or flat night layout. */
        static void BuildGeometry(UWorld* InWorld, bool bNight);

        /** Directional Stationary + Static point/spot fill for Lightmass. */
        static void BuildLighting(UWorld* InWorld, bool bNight);

        /** Geometry + lighting + sky/world-settings stubs used by procedural fallback. */
        static void PopulateArena(UWorld* InWorld, bool bNight);
    };

} // namespace Leon
