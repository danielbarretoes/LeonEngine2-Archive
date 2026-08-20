#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <string>

namespace Leon::Editor {

    /**
     * @brief Level viewport panel: docks into ImGui, handles camera navigation,
     * and displays FWorldRenderer output.
     */
    class FViewportPanel {
    public:
        FViewportPanel() = default;

        void Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor);
        void FocusOnActor(AActor* InActor);

    private:
        void EnsureCamera();
        void ProcessCameraInput();
        void RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight);

        FPerspectiveCamera EditorCamera{45.0f, 1.778f, 0.1f, 1000.0f};
        bool bCameraReady = false;
        bool bIsViewportFocused = false;
        bool bIsViewportHovered = false;

        float CameraSpeed = 8.0f;
        glm::vec2 LastMousePos{0.0f, 0.0f};

        uint32_t LastWidth = 0;
        uint32_t LastHeight = 0;
    };

} // namespace Leon::Editor
