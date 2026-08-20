#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace Leon::Editor {

    void FTransformGizmo::ProcessHotkeys() {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            // Right-click is camera free-fly; ignore gizmo hotkeys
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
            CurrentOperation = EGizmoOperation::Select;
        } else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            CurrentOperation = EGizmoOperation::Translate;
        } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            CurrentOperation = EGizmoOperation::Rotate;
        } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            CurrentOperation = EGizmoOperation::Scale;
        }
    }

    glm::vec2 FTransformGizmo::WorldToScreen(const glm::vec3& InWorldPos, const glm::mat4& InViewProj, float InVx,
                                             float InVy, float InVw, float InVh, bool& OutInFront) const {
        glm::vec4 clip = InViewProj * glm::vec4(InWorldPos, 1.0f);
        OutInFront = (clip.w > 0.001f);
        if (!OutInFront) {
            return glm::vec2(-1000.0f);
        }

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = InVx + (ndc.x * 0.5f + 0.5f) * InVw;
        float sy = InVy + (1.0f - (ndc.y * 0.5f + 0.5f)) * InVh;
        return glm::vec2(sx, sy);
    }

    void FTransformGizmo::Draw(AActor* InSelectedActor, const FPerspectiveCamera& InCamera, float InViewportX,
                               float InViewportY, float InViewportW, float InViewportH) {
        if (!InSelectedActor || !InSelectedActor->HasComponent<FTransformComponent>()) {
            ActiveAxis = EGizmoAxis::None;
            return;
        }

        ProcessHotkeys();

        if (CurrentOperation == EGizmoOperation::Select) {
            ActiveAxis = EGizmoAxis::None;
            return;
        }

        auto& tc = InSelectedActor->GetComponent<FTransformComponent>();
        glm::vec3 actorPos = tc.Translation;
        glm::mat4 viewProj = InCamera.GetProjectionMatrix() * InCamera.GetViewMatrix();

        bool bOriginInFront = false;
        glm::vec2 originScreen =
            WorldToScreen(actorPos, viewProj, InViewportX, InViewportY, InViewportW, InViewportH, bOriginInFront);
        if (!bOriginInFront) {
            return;
        }

        // Distance-adaptive gizmo scale
        float distToCam = glm::length(InCamera.GetPosition() - actorPos);
        float axisLength = std::max(0.6f, distToCam * 0.15f);

        // Direction vectors (World vs Local)
        glm::vec3 dirX = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 dirY = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 dirZ = glm::vec3(0.0f, 0.0f, 1.0f);

        if (CurrentMode == EGizmoMode::Local) {
            glm::mat4 rotMatrix = glm::mat4(1.0f);
            rotMatrix = glm::rotate(rotMatrix, glm::radians(tc.Rotation.z), glm::vec3(0, 0, 1));
            rotMatrix = glm::rotate(rotMatrix, glm::radians(tc.Rotation.y), glm::vec3(0, 1, 0));
            rotMatrix = glm::rotate(rotMatrix, glm::radians(tc.Rotation.x), glm::vec3(1, 0, 0));
            dirX = glm::vec3(rotMatrix * glm::vec4(1, 0, 0, 0));
            dirY = glm::vec3(rotMatrix * glm::vec4(0, 1, 0, 0));
            dirZ = glm::vec3(rotMatrix * glm::vec4(0, 0, 1, 0));
        }

        bool bXInFront = false, bYInFront = false, bZInFront = false;
        glm::vec2 posXScreen = WorldToScreen(actorPos + dirX * axisLength, viewProj, InViewportX, InViewportY,
                                             InViewportW, InViewportH, bXInFront);
        glm::vec2 posYScreen = WorldToScreen(actorPos + dirY * axisLength, viewProj, InViewportX, InViewportY,
                                             InViewportW, InViewportH, bYInFront);
        glm::vec2 posZScreen = WorldToScreen(actorPos + dirZ * axisLength, viewProj, InViewportX, InViewportY,
                                             InViewportW, InViewportH, bZInFront);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 mousePos = ImGui::GetMousePos();
        glm::vec2 mouse(mousePos.x, mousePos.y);

        // Interaction Hit Detection
        auto DistToSegment = [](glm::vec2 p, glm::vec2 a, glm::vec2 b) -> float {
            glm::vec2 ab = b - a;
            float lenSq = glm::dot(ab, ab);
            if (lenSq < 0.001f)
                return glm::length(p - a);
            float t = std::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
            glm::vec2 proj = a + ab * t;
            return glm::length(p - proj);
        };

        float hoverDistX = DistToSegment(mouse, originScreen, posXScreen);
        float hoverDistY = DistToSegment(mouse, originScreen, posYScreen);
        float hoverDistZ = DistToSegment(mouse, originScreen, posZScreen);

        EGizmoAxis hoveredAxis = EGizmoAxis::None;
        float minHover = 10.0f;

        if (hoverDistX < minHover && hoverDistX <= hoverDistY && hoverDistX <= hoverDistZ) {
            hoveredAxis = EGizmoAxis::X;
        } else if (hoverDistY < minHover && hoverDistY <= hoverDistX && hoverDistY <= hoverDistZ) {
            hoveredAxis = EGizmoAxis::Y;
        } else if (hoverDistZ < minHover) {
            hoveredAxis = EGizmoAxis::Z;
        }

        // Handle Drag Start
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis != EGizmoAxis::None &&
            ActiveAxis == EGizmoAxis::None) {
            ActiveAxis = hoveredAxis;
            DragStartMouse = mouse;
            InitialActorLocation = tc.Translation;
            InitialActorRotation = tc.Rotation;
            InitialActorScale = tc.Scale;
        }

        // Handle Dragging
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ActiveAxis != EGizmoAxis::None) {
            glm::vec2 deltaMouse = mouse - DragStartMouse;
            float dragFactor = distToCam * 0.003f;

            if (CurrentOperation == EGizmoOperation::Translate) {
                glm::vec3 deltaWorld(0.0f);
                if (ActiveAxis == EGizmoAxis::X) {
                    glm::vec2 axisDir2D = glm::normalize(posXScreen - originScreen);
                    float proj = glm::dot(deltaMouse, axisDir2D);
                    deltaWorld = dirX * (proj * dragFactor);
                } else if (ActiveAxis == EGizmoAxis::Y) {
                    glm::vec2 axisDir2D = glm::normalize(posYScreen - originScreen);
                    float proj = glm::dot(deltaMouse, axisDir2D);
                    deltaWorld = dirY * (proj * dragFactor);
                } else if (ActiveAxis == EGizmoAxis::Z) {
                    glm::vec2 axisDir2D = glm::normalize(posZScreen - originScreen);
                    float proj = glm::dot(deltaMouse, axisDir2D);
                    deltaWorld = dirZ * (proj * dragFactor);
                }

                glm::vec3 newLoc = InitialActorLocation + deltaWorld;
                if (bSnapEnabled && TranslationSnap > 0.001f) {
                    newLoc.x = std::round(newLoc.x / TranslationSnap) * TranslationSnap;
                    newLoc.y = std::round(newLoc.y / TranslationSnap) * TranslationSnap;
                    newLoc.z = std::round(newLoc.z / TranslationSnap) * TranslationSnap;
                }
                InSelectedActor->SetActorLocation(newLoc);

            } else if (CurrentOperation == EGizmoOperation::Rotate) {
                float rotAngle = (deltaMouse.x - deltaMouse.y) * 0.5f;
                glm::vec3 newRot = InitialActorRotation;

                if (ActiveAxis == EGizmoAxis::X)
                    newRot.x += rotAngle;
                else if (ActiveAxis == EGizmoAxis::Y)
                    newRot.y += rotAngle;
                else if (ActiveAxis == EGizmoAxis::Z)
                    newRot.z += rotAngle;

                if (bSnapEnabled && RotationSnap > 0.001f) {
                    newRot.x = std::round(newRot.x / RotationSnap) * RotationSnap;
                    newRot.y = std::round(newRot.y / RotationSnap) * RotationSnap;
                    newRot.z = std::round(newRot.z / RotationSnap) * RotationSnap;
                }
                InSelectedActor->SetActorRotation(newRot);

            } else if (CurrentOperation == EGizmoOperation::Scale) {
                float scaleFactor = 1.0f + (deltaMouse.x - deltaMouse.y) * 0.01f;
                glm::vec3 newScale = InitialActorScale;

                if (ActiveAxis == EGizmoAxis::X)
                    newScale.x = std::max(0.01f, InitialActorScale.x * scaleFactor);
                else if (ActiveAxis == EGizmoAxis::Y)
                    newScale.y = std::max(0.01f, InitialActorScale.y * scaleFactor);
                else if (ActiveAxis == EGizmoAxis::Z)
                    newScale.z = std::max(0.01f, InitialActorScale.z * scaleFactor);

                if (bSnapEnabled && ScaleSnap > 0.001f) {
                    newScale.x = std::round(newScale.x / ScaleSnap) * ScaleSnap;
                    newScale.y = std::round(newScale.y / ScaleSnap) * ScaleSnap;
                    newScale.z = std::round(newScale.z / ScaleSnap) * ScaleSnap;
                }
                InSelectedActor->SetActorScale(newScale);
            }
        }

        // Handle Drag Release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            ActiveAxis = EGizmoAxis::None;
        }

        // Render Gizmo Visuals
        ImU32 colX = (ActiveAxis == EGizmoAxis::X || hoveredAxis == EGizmoAxis::X) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(235, 50, 60, 240);
        ImU32 colY = (ActiveAxis == EGizmoAxis::Y || hoveredAxis == EGizmoAxis::Y) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(50, 220, 70, 240);
        ImU32 colZ = (ActiveAxis == EGizmoAxis::Z || hoveredAxis == EGizmoAxis::Z) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(60, 130, 245, 240);

        float lineThick = 2.5f;

        // X Axis Line & Handle
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posXScreen.x, posXScreen.y), colX, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posXScreen.x, posXScreen.y), 4.5f, colX);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posXScreen.x - 3.5f, posXScreen.y - 3.5f),
                                    ImVec2(posXScreen.x + 3.5f, posXScreen.y + 3.5f), colX);
        }

        // Y Axis Line & Handle
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posYScreen.x, posYScreen.y), colY, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posYScreen.x, posYScreen.y), 4.5f, colY);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posYScreen.x - 3.5f, posYScreen.y - 3.5f),
                                    ImVec2(posYScreen.x + 3.5f, posYScreen.y + 3.5f), colY);
        }

        // Z Axis Line & Handle
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posZScreen.x, posZScreen.y), colZ, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posZScreen.x, posZScreen.y), 4.5f, colZ);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posZScreen.x - 3.5f, posZScreen.y - 3.5f),
                                    ImVec2(posZScreen.x + 3.5f, posZScreen.y + 3.5f), colZ);
        }

        // Origin Pivot Hub
        drawList->AddCircleFilled(ImVec2(originScreen.x, originScreen.y), 4.0f, IM_COL32(240, 240, 245, 255));
    }

} // namespace Leon::Editor
