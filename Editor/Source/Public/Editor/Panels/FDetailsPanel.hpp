#pragma once

#include "Core/Base.hpp"
#include "Gameplay/AActor.hpp"

namespace Leon::Editor {

    /**
     * @brief Details / Inspector panel for modifying actor properties, components,
     * and local vs world transforms.
     */
    class FDetailsPanel {
    public:
        FDetailsPanel() = default;

        void Draw(AActor* InSelectedActor);

    private:
        void DrawTransformComponent(AActor& InActor);
        void DrawStaticMeshComponent(AActor& InActor);
        void DrawLightComponents(AActor& InActor);
        void DrawCameraComponent(AActor& InActor);
        void DrawAddComponentMenu(AActor& InActor);

        bool bLocalTransformMode = false;
    };

} // namespace Leon::Editor
