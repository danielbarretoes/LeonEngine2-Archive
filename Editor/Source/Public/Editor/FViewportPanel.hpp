#pragma once

#include "Core/Base.hpp"
#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "Renderer/FWorldRenderer.hpp"

#include <memory>
#include <string>

namespace Leon::Editor {

    enum class EViewportShadingMode { Lit, Unlit, Wireframe };

    enum class EViewportViewMode { Perspective, Top, Front, Side };

    /**
     * @brief Level viewport panel: docks into ImGui, handles camera navigation,
     * viewport overlays, camera controls, gizmos, and displays FWorldRenderer output.
     */
    class FViewportPanel {
    public:
        FViewportPanel() = default;

        void Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor);
        void FocusOnActor(AActor* InActor);

        FPerspectiveCamera& GetCamera() { return EditorCamera; }
        const FPerspectiveCamera& GetCamera() const { return EditorCamera; }

    private:
        void EnsureCamera();
        void ProcessCameraInput();
        void RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight);
        void DrawViewportOverlay(const std::string& InMapName, AActor* InSelectedActor, uint32_t InActorCount);

        FPerspectiveCamera EditorCamera{45.0f, 1.778f, 0.1f, 1000.0f};
        FTransformGizmo Gizmo;

        std::unique_ptr<FWorldRenderer> WorldRenderer;
        UWorld* WorldRendererWorld = nullptr;

        bool bCameraReady = false;
        bool bIsViewportFocused = false;
        bool bIsViewportHovered = false;

        EViewportShadingMode ShadingMode = EViewportShadingMode::Lit;
        EViewportViewMode ViewMode = EViewportViewMode::Perspective;
        bool bShowGrid = true;
        bool bShowSelectionBounds = true;
        bool bShowStatsOverlay = true;

        float CameraSpeed = 8.0f;
        glm::vec2 LastMousePos{0.0f, 0.0f};

        uint32_t LastWidth = 0;
        uint32_t LastHeight = 0;
    };

} // namespace Leon::Editor
