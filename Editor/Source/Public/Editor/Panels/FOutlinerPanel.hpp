#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <functional>
#include <string>

namespace Leon::Editor {

    /**
     * @brief World Outliner panel showing all scene actors, hierarchical parenting,
     * type badges with Lucide icons, visibility, selection, and context menus.
     */
    class FOutlinerPanel {
    public:
        using FOnActorSelected = std::function<void(AActor* InActor)>;
        using FOnActorFocus = std::function<void(AActor* InActor)>;

        FOutlinerPanel() = default;

        void SetOnActorSelected(FOnActorSelected InCallback) { OnActorSelected = std::move(InCallback); }
        void SetOnActorFocus(FOnActorFocus InCallback) { OnActorFocus = std::move(InCallback); }

        void SetSelectedActor(AActor* InActor) { SelectedActor = InActor; }
        AActor* GetSelectedActor() const { return SelectedActor; }

        void Draw(UWorld* InWorld);

    private:
        void DrawActorNode(UWorld& InWorld, AActor* InActor, const std::string& InFilter);
        void DrawContextMenu(UWorld& InWorld, AActor* InActor);
        void SpawnNewActor(UWorld& InWorld, const std::string& InType);

        FOnActorSelected OnActorSelected;
        FOnActorFocus OnActorFocus;

        AActor* SelectedActor = nullptr;
        char FilterBuffer[128] = "";
    };

} // namespace Leon::Editor
