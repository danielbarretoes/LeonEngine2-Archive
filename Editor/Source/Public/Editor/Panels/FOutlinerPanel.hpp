#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <string>
#include <functional>

namespace Leon::Editor {

    /**
     * @brief World Outliner panel listing actors in the current level.
     */
    class FOutlinerPanel {
    public:
        using FOnActorSelected = std::function<void(AActor* InActor)>;
        using FOnActorFocus = std::function<void(AActor* InActor)>;

        FOutlinerPanel() = default;

        void SetOnActorSelected(FOnActorSelected InCallback) { OnActorSelected = std::move(InCallback); }
        void SetOnActorFocus(FOnActorFocus InCallback) { OnActorFocus = std::move(InCallback); }

        AActor* GetSelectedActor() const { return SelectedActor; }
        void SetSelectedActor(AActor* InActor) { SelectedActor = InActor; }

        void Draw(UWorld* InWorld);

    private:
        void DrawActorTree(UWorld& InWorld);
        void DrawContextMenu(UWorld& InWorld, AActor* InActor);
        void SpawnNewActor(UWorld& InWorld, const std::string& InType);

        AActor* SelectedActor = nullptr;
        FOnActorSelected OnActorSelected;
        FOnActorFocus OnActorFocus;
        char FilterBuffer[128] = "";
    };

} // namespace Leon::Editor
