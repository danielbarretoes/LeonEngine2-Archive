#pragma once

#include "Core/Base.hpp"
#include "Editor/Subsystems/FEditorSelectionSubsystem.hpp"
#include "Editor/Subsystems/FEditorTransactionSubsystem.hpp"
#include "Gameplay/AActor.hpp"

#include <string>
#include <vector>

namespace Leon::Editor {

    /**
     * @brief Details / Inspector panel for modifying actor properties, components,
     * materials, lights, and transform coordinates with multi-selection support and drag & drop.
     */
    class FDetailsPanel {
    public:
        FDetailsPanel() = default;

        void SetSelectionSubsystem(FEditorSelectionSubsystem* InSubsystem) { SelectionSubsystem = InSubsystem; }
        void SetTransactionSubsystem(FEditorTransactionSubsystem* InSubsystem) { TransactionSubsystem = InSubsystem; }

        void Draw(AActor* InSelectedActor, bool* bInOutOpen = nullptr);

    private:
        void DrawSingleActorDetails(AActor& InActor, const std::string& InFilter);
        void DrawMultiActorDetails(const std::vector<AActor*>& InActors, const std::string& InFilter);

        void DrawTransformComponent(AActor& InActor, const std::string& InFilter);
        void DrawStaticMeshComponent(AActor& InActor, const std::string& InFilter);
        void DrawMaterialComponent(AActor& InActor, const std::string& InFilter);
        void DrawLightComponents(AActor& InActor, const std::string& InFilter);
        void DrawCameraComponent(AActor& InActor, const std::string& InFilter);
        void DrawBoxCollisionComponent(AActor& InActor, const std::string& InFilter);
        void DrawAddComponentMenu(AActor& InActor);

        FEditorSelectionSubsystem* SelectionSubsystem = nullptr;
        FEditorTransactionSubsystem* TransactionSubsystem = nullptr;

        char SearchBuffer[128] = "";
        bool bLocalTransformMode = false;
    };

} // namespace Leon::Editor
