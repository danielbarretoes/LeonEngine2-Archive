#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <string>

namespace Leon::Editor {

    /**
     * @brief Place Actors palette panel for quickly spawning actors into the active level
     * with drag-and-drop support to the Viewport.
     */
    class FPlaceActorsPanel {
    public:
        using FOnActorSpawned = std::function<void(AActor* InActor)>;

        FPlaceActorsPanel() = default;

        void SetOnActorSpawned(FOnActorSpawned InCallback) { OnActorSpawned = std::move(InCallback); }

        void Draw(UWorld* InWorld, bool* bInOutOpen = nullptr);

        static AActor* SpawnActorAt(UWorld& InWorld, const std::string& InType, const glm::vec3& InLocation);

    private:
        void SpawnActor(UWorld& InWorld, const std::string& InType);

        FOnActorSpawned OnActorSpawned;
        int SelectedCategory = 0; // 0: Basic, 1: Lights, 2: Shapes, 3: Volumes
    };

} // namespace Leon::Editor
