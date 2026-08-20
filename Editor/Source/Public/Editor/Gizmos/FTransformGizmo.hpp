#pragma once

#include "Core/Base.hpp"
#include "Editor/Commands/FTransformActorsCommand.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon::Editor {

    class FEditorHistory;

    enum class EGizmoOperation { Select, Translate, Rotate, Scale };
    enum class EGizmoMode { World, Local };

    enum class EGizmoAxis { None, X, Y, Z, PlaneXY, PlaneXZ, PlaneYZ, Uniform };

    /**
     * @brief 3D viewport gizmo controller for manipulating actor transforms (Translate, Rotate, Scale)
     * with axis & plane dragging, snapping, and world/local coordinate support.
     */
    class FTransformGizmo {
    public:
        FTransformGizmo() = default;

        EGizmoOperation GetOperation() const { return CurrentOperation; }
        void SetOperation(EGizmoOperation InOp) { CurrentOperation = InOp; }

        EGizmoMode GetMode() const { return CurrentMode; }
        void SetMode(EGizmoMode InMode) { CurrentMode = InMode; }

        bool IsSnappingEnabled() const { return bSnapEnabled; }
        void SetSnappingEnabled(bool bEnabled) { bSnapEnabled = bEnabled; }

        float GetTranslationSnap() const { return TranslationSnap; }
        void SetTranslationSnap(float InSnap) { TranslationSnap = InSnap; }

        float GetRotationSnap() const { return RotationSnap; }
        void SetRotationSnap(float InSnap) { RotationSnap = InSnap; }

        float GetScaleSnap() const { return ScaleSnap; }
        void SetScaleSnap(float InSnap) { ScaleSnap = InSnap; }

        bool IsDragging() const { return ActiveAxis != EGizmoAxis::None; }
        bool IsHovered() const { return bHovered; }

        /** Clears drag/hover so viewport picking works again after selection changes. */
        void CancelInteraction() {
            ActiveAxis = EGizmoAxis::None;
            bHovered = false;
            DragBefore.clear();
            bDragRecorded = false;
        }

        void ProcessHotkeys();

        /**
         * @param InActors Selected actors (primary is pivot). History receives undo on drag end.
         */
        void Draw(const std::vector<AActor*>& InActors, const FPerspectiveCamera& InCamera, float InViewportX,
                  float InViewportY, float InViewportW, float InViewportH, FEditorHistory* InHistory);

        /** Legacy single-actor draw (no undo). Prefer the multi-actor overload. */
        void Draw(AActor* InSelectedActor, const FPerspectiveCamera& InCamera, float InViewportX, float InViewportY,
                  float InViewportW, float InViewportH);

    private:
        glm::vec2 WorldToScreen(const glm::vec3& InWorldPos, const glm::mat4& InViewProj, float InVx, float InVy,
                                float InVw, float InVh, bool& OutInFront) const;

        void CommitDragIfNeeded(FEditorHistory* InHistory);

        EGizmoOperation CurrentOperation = EGizmoOperation::Translate;
        EGizmoMode CurrentMode = EGizmoMode::World;

        bool bSnapEnabled = false;
        float TranslationSnap = 10.0f;
        float RotationSnap = 15.0f;
        float ScaleSnap = 0.25f;

        EGizmoAxis ActiveAxis = EGizmoAxis::None;
        bool bHovered = false;
        glm::vec2 DragStartMouse = glm::vec2(0.0f);
        glm::vec3 InitialActorLocation = glm::vec3(0.0f);
        glm::vec3 InitialActorRotation = glm::vec3(0.0f);
        glm::vec3 InitialActorScale = glm::vec3(1.0f);

        std::vector<FActorTransformState> DragBefore;
        bool bDragRecorded = false;
    };

} // namespace Leon::Editor
