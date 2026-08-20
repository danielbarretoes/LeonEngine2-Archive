#pragma once

#include "Core/Base.hpp"
#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Editor/Subsystems/FEditorSelectionSubsystem.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "Renderer/FWorldRenderer.hpp"

#include <functional>
#include <glm/glm.hpp>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

namespace Leon::Editor {

    enum class EViewportShadingMode { Lit, Unlit, Wireframe };
    enum class EViewportViewMode { Perspective, Top, Front, Side };

    /**
     * @brief Level viewport panel: docks into ImGui, handles Unreal-style camera navigation,
     * transform tools (Select, Move, Rotate, Scale), snapping, marquee box selection,
     * 3D gizmos, drag & drop actor/material spawning, and FWorldRenderer output.
     */
    class FViewportPanel {
    public:
        using FOnActorSelected = std::function<void(AActor* InActor)>;
        using FOnActorSpawned = std::function<void(AActor* InActor)>;

        FViewportPanel() = default;

        void SetSelectionSubsystem(FEditorSelectionSubsystem* InSubsystem) { SelectionSubsystem = InSubsystem; }
        void SetOnActorSelected(FOnActorSelected InCallback) { OnActorSelected = std::move(InCallback); }
        void SetOnActorSpawned(FOnActorSpawned InCallback) { OnActorSpawned = std::move(InCallback); }

        void Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor, bool* bInOutOpen = nullptr);
        void FocusOnActor(AActor* InActor);

        FPerspectiveCamera& GetCamera() { return EditorCamera; }
        const FPerspectiveCamera& GetCamera() const { return EditorCamera; }

    private:
        void EnsureCamera();
        void ProcessCameraInput();
        void RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight);
        void DrawViewportToolbar();
        void DrawViewportOverlay(const std::string& InMapName, AActor* InSelectedActor, uint32_t InActorCount);

        AActor* PickActorAtScreenPos(UWorld& InWorld, const glm::vec2& InScreenPos, const ImVec2& InViewportMin,
                                     const ImVec2& InViewportSize);
        void ProcessMarqueeSelection(UWorld& InWorld, const ImVec2& InViewportMin, const ImVec2& InViewportSize);

        void DrawSelectionOutline(AActor* InSelectedActor, const ImVec2& InViewportMin, const ImVec2& InViewportSize);
        void DrawPlacementGhost(const glm::vec3& InWorldPos, const ImVec2& InViewportMin, const ImVec2& InViewportSize,
                                const char* InLabel);
        glm::vec3 GetWorldRayIntersection(const glm::vec2& InScreenPos, const ImVec2& InViewportMin,
                                          const ImVec2& InViewportSize);

        FPerspectiveCamera EditorCamera{45.0f, 1.778f, 0.1f, 1000.0f};
        FTransformGizmo Gizmo;

        FEditorSelectionSubsystem* SelectionSubsystem = nullptr;
        FOnActorSelected OnActorSelected;
        FOnActorSpawned OnActorSpawned;

        std::unique_ptr<FWorldRenderer> WorldRenderer;

        EViewportShadingMode ShadingMode = EViewportShadingMode::Lit;
        EViewportViewMode ViewMode = EViewportViewMode::Perspective;

        float CameraSpeed = 8.0f;
        bool bCameraInitialized = false;
        bool bShowStatistics = true;

        // Marquee Selection Box State
        bool bMarqueeSelecting = false;
        ImVec2 MarqueeStart = ImVec2(0, 0);
        ImVec2 MarqueeEnd = ImVec2(0, 0);

        // Frame rate tracking
        float FrameRate = 60.0f;
        float FrameTimeMs = 16.6f;
    };

} // namespace Leon::Editor
