#pragma once

#include "Engine/EMobility.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class UWorld;
    class AActor;

    /**
     * Procedural collision / mesh / light helpers for labs and arena builders.
     * Product material paths stay in the game wrapper.
     */
    struct FProceduralPrimitiveSpawner {
        static AActor* SpawnStaticBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                      const glm::vec3& InScale);

        /**
         * Collision box + PBR box mesh with world-meter UVs.
         * InScale is full world size (meters). InUvTile is meters-per-UV-repeat (texture density); 0 keeps 1 m/tile.
         */
        static AActor* SpawnMeshBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                    const glm::vec3& InScale, const glm::vec3& InColor = glm::vec3(1.0f),
                                    const std::string& InMaterialPath = {}, float InMetersPerUv = 1.0f,
                                    bool bUsePlanarReflection = false, bool bVisibleInReflection = true);

        static AActor* SpawnPointLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                       const glm::vec3& InColor, float InIntensity, float InRadius,
                                       ELightMobility InMobility = ELightMobility::Stationary);

        static AActor* SpawnSpotLight(UWorld* InWorld, const std::string& InName, const glm::vec3& InPos,
                                      const glm::vec3& InDir, const glm::vec3& InColor, float InIntensity,
                                      float InRadius, float InInnerDeg, float InOuterDeg,
                                      ELightMobility InMobility = ELightMobility::Stationary);
    };

} // namespace Leon
