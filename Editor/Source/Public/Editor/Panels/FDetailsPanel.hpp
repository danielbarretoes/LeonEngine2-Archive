#pragma once

#include "Core/Base.hpp"
#include "Editor/Commands/FTransformActorsCommand.hpp"
#include "Editor/Context/FEditorContext.hpp"
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

        void SetEditorContext(FEditorContext* InContext) { Context = InContext; }

        void Draw(AActor* InSelectedActor, bool* bInOutOpen = nullptr);

    private:
        void DrawSingleActorDetails(AActor& InActor, const std::string& InFilter);
        void DrawMultiActorDetails(const std::vector<AActor*>& InActors, const std::string& InFilter);

        void DrawTransformComponent(AActor& InActor, const std::string& InFilter);
        void DrawPlayerStartProperties(AActor& InActor, const std::string& InFilter);
        void DrawStaticMeshComponent(AActor& InActor, const std::string& InFilter);
        void DrawMaterialComponent(AActor& InActor, const std::string& InFilter);
        void DrawLightComponents(AActor& InActor, const std::string& InFilter);
        void DrawCameraComponent(AActor& InActor, const std::string& InFilter);
        void DrawBoxCollisionComponent(AActor& InActor, const std::string& InFilter);
        void DrawAddComponentMenu(AActor& InActor);

        void BeginTransformUndoCapture(const std::vector<AActor*>& InActors);
        void CommitTransformUndoIfIdle();

        FEditorContext* Context = nullptr;

        char SearchBuffer[128] = "";
        bool bLocalTransformMode = false;

        bool bTransformUndoPending = false;
        std::vector<FActorTransformState> TransformUndoBefore;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FDetailsPanel;
}
