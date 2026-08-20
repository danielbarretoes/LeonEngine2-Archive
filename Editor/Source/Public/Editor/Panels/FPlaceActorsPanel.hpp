#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include <functional>

namespace Leon::Editor {

    /**
     * @brief Place Actors palette panel for quickly spawning actors into the active level.
     */
    class FPlaceActorsPanel {
    public:
        using FOnActorSpawned = std::function<void(AActor* InActor)>;

        FPlaceActorsPanel() = default;

        void SetOnActorSpawned(FOnActorSpawned InCallback) { OnActorSpawned = std::move(InCallback); }

        void Draw(UWorld* InWorld);

    private:
        void SpawnActor(UWorld& InWorld, const std::string& InType);

        FOnActorSpawned OnActorSpawned;
        int SelectedCategory = 0; // 0: Basic, 1: Lights, 2: Shapes, 3: Volumes
    };

} // namespace Leon::Editor
