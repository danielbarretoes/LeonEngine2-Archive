#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Engine/Components.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FTransformGizmo::ProcessHotkeys() {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            // Right-click is camera free-fly; do not switch gizmos
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            CurrentOperation = EGizmoOperation::Translate;
        } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            CurrentOperation = EGizmoOperation::Rotate;
        } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            CurrentOperation = EGizmoOperation::Scale;
        }
    }

    void FTransformGizmo::Draw(AActor* InSelectedActor, const FPerspectiveCamera& InCamera, float InViewportX,
                               float InViewportY, float InViewportW, float InViewportH) {
        (void)InCamera;
        (void)InViewportX;
        (void)InViewportY;
        (void)InViewportW;
        (void)InViewportH;

        if (!InSelectedActor || !InSelectedActor->HasComponent<FTransformComponent>()) {
            return;
        }

        ProcessHotkeys();
    }

} // namespace Leon::Editor
