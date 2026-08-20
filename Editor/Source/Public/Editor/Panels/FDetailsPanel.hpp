#pragma once

#include "Core/Base.hpp"
#include "Gameplay/AActor.hpp"

namespace Leon::Editor {

    /**
     * @brief Details / Inspector panel for modifying actor properties and components.
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
    };

} // namespace Leon::Editor
