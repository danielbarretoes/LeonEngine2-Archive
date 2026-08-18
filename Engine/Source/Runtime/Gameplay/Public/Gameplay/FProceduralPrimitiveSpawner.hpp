#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class UWorld;
    class AActor;

    /** Collision box spawn used by tests and game arena builders. Visuals stay with the caller. */
    struct FProceduralPrimitiveSpawner {
        static AActor* SpawnStaticBox(UWorld* InWorld, const std::string& InName, const glm::vec3& InLocation,
                                      const glm::vec3& InScale);
    };

} // namespace Leon
