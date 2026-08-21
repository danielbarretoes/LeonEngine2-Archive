#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Editor/Context/FEditorHistory.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace Leon::Editor {

    void FTransformGizmo::ProcessHotkeys() {
        if (ImGui::GetIO().WantTextInput) {
            return;
        }

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

    bool FTransformGizmo::ScreenToWorldRay(const FPerspectiveCamera& InCamera, const glm::vec2& InMouse, float InVx,
                                           float InVy, float InVw, float InVh, glm::vec3& OutOrigin,
                                           glm::vec3& OutDir) const {
        if (InVw <= 1e-4f || InVh <= 1e-4f)
            return false;

        const float relX = (InMouse.x - InVx) / InVw;
        const float relY = (InMouse.y - InVy) / InVh;
        const float ndcX = relX * 2.0f - 1.0f;
        const float ndcY = 1.0f - relY * 2.0f;

        const glm::mat4 invVP = glm::inverse(InCamera.GetProjectionMatrix() * InCamera.GetViewMatrix());
        glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < 1e-6f || std::abs(farPoint.w) < 1e-6f)
            return false;

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;
        OutOrigin = glm::vec3(nearPoint);
        const glm::vec3 delta = glm::vec3(farPoint - nearPoint);
        const float len = glm::length(delta);
        if (len < 1e-6f)
            return false;
        OutDir = delta / len;
        return true;
    }

    bool FTransformGizmo::IntersectRayWithAxis(const glm::vec3& InRayOrigin, const glm::vec3& InRayDir,
                                               glm::vec3& OutHit) const {
        const float denom = glm::dot(DragPlaneNormal, InRayDir);
        if (std::abs(denom) < 1e-6f)
            return false;
        const float t = glm::dot(DragAxisOrigin - InRayOrigin, DragPlaneNormal) / denom;
        if (t < 0.0f)
            return false;
        OutHit = InRayOrigin + InRayDir * t;
        return true;
    }

    void FTransformGizmo::CommitDragIfNeeded(FEditorHistory* InHistory) {
        if (!bDragRecorded || !InHistory || DragBefore.empty()) {
            DragBefore.clear();
            bDragRecorded = false;
            return;
        }

        std::vector<FActorTransformState> after;
        after.reserve(DragBefore.size());
        bool bAnyChange = false;
        for (const auto& before : DragBefore) {
            FActorTransformState state = FTransformActorsCommand::Capture(before.Actor);
            after.push_back(state);
            if (FTransformActorsCommand::Differ(before, state))
                bAnyChange = true;
        }

        if (bAnyChange) {
            std::string desc = "Transform Actors";
            if (CurrentOperation == EGizmoOperation::Translate)
                desc = "Move Actors";
            else if (CurrentOperation == EGizmoOperation::Rotate)
                desc = "Rotate Actors";
            else if (CurrentOperation == EGizmoOperation::Scale)
                desc = "Scale Actors";

            InHistory->PushExecutedCommand(
                std::make_unique<FTransformActorsCommand>(std::move(DragBefore), std::move(after), std::move(desc)));
        }

        DragBefore.clear();
        bDragRecorded = false;
    }

    void FTransformGizmo::Draw(AActor* InSelectedActor, const FPerspectiveCamera& InCamera, float InViewportX,
                               float InViewportY, float InViewportW, float InViewportH) {
        std::vector<AActor*> actors;
        if (InSelectedActor)
            actors.push_back(InSelectedActor);
        Draw(actors, InCamera, InViewportX, InViewportY, InViewportW, InViewportH, nullptr);
    }

    void FTransformGizmo::Draw(const std::vector<AActor*>& InActors, const FPerspectiveCamera& InCamera,
                               float InViewportX, float InViewportY, float InViewportW, float InViewportH,
                               FEditorHistory* InHistory) {
        AActor* primary = nullptr;
        for (AActor* actor : InActors) {
            if (actor && !actor->IsPendingKill() && actor->HasComponent<FTransformComponent>()) {
                primary = actor;
                break;
            }
        }

        if (!primary) {
            CancelInteraction();
            return;
        }

        ProcessHotkeys();

        if (CurrentOperation == EGizmoOperation::Select) {
            ActiveAxis = EGizmoAxis::None;
            bHovered = false;
            return;
        }

        auto& tc = primary->GetComponent<FTransformComponent>();
        glm::vec3 actorPos = tc.Translation;
        glm::mat4 viewProj = InCamera.GetProjectionMatrix() * InCamera.GetViewMatrix();

        bool bOriginInFront = false;
        glm::vec2 originScreen =
            WorldToScreen(actorPos, viewProj, InViewportX, InViewportY, InViewportW, InViewportH, bOriginInFront);
        if (!bOriginInFront) {
            bHovered = false;
            return;
        }

        float distToCam = glm::length(InCamera.GetPosition() - actorPos);
        float axisLength = std::max(0.6f, distToCam * 0.15f);

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

        bHovered = (hoveredAxis != EGizmoAxis::None || ActiveAxis != EGizmoAxis::None);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis != EGizmoAxis::None &&
            ActiveAxis == EGizmoAxis::None) {
            ActiveAxis = hoveredAxis;
            DragStartMouse = mouse;
            InitialActorLocation = tc.Translation;
            InitialActorRotation = tc.Rotation;
            InitialActorScale = tc.Scale;

            DragBefore.clear();
            for (AActor* actor : InActors) {
                if (!actor || actor->IsPendingKill() || !actor->HasComponent<FTransformComponent>())
                    continue;
                DragBefore.push_back(FTransformActorsCommand::Capture(actor));
            }
            bDragRecorded = !DragBefore.empty();
        }

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

                for (const auto& before : DragBefore) {
                    if (!before.Actor || before.Actor->IsPendingKill())
                        continue;
                    glm::vec3 newLoc = before.Location + deltaWorld;
                    if (bSnapEnabled && TranslationSnap > 0.001f) {
                        newLoc.x = std::round(newLoc.x / TranslationSnap) * TranslationSnap;
                        newLoc.y = std::round(newLoc.y / TranslationSnap) * TranslationSnap;
                        newLoc.z = std::round(newLoc.z / TranslationSnap) * TranslationSnap;
                    }
                    before.Actor->SetActorLocation(newLoc);
                }

            } else if (CurrentOperation == EGizmoOperation::Rotate) {
                float rotAngle = (deltaMouse.x - deltaMouse.y) * 0.5f;
                glm::vec3 rotDelta(0.0f);
                if (ActiveAxis == EGizmoAxis::X)
                    rotDelta.x = rotAngle;
                else if (ActiveAxis == EGizmoAxis::Y)
                    rotDelta.y = rotAngle;
                else if (ActiveAxis == EGizmoAxis::Z)
                    rotDelta.z = rotAngle;

                for (const auto& before : DragBefore) {
                    if (!before.Actor || before.Actor->IsPendingKill())
                        continue;
                    glm::vec3 newRot = before.Rotation + rotDelta;
                    if (bSnapEnabled && RotationSnap > 0.001f) {
                        newRot.x = std::round(newRot.x / RotationSnap) * RotationSnap;
                        newRot.y = std::round(newRot.y / RotationSnap) * RotationSnap;
                        newRot.z = std::round(newRot.z / RotationSnap) * RotationSnap;
                    }
                    before.Actor->SetActorRotation(newRot);
                }

            } else if (CurrentOperation == EGizmoOperation::Scale) {
                float scaleFactor = 1.0f + (deltaMouse.x - deltaMouse.y) * 0.01f;

                for (const auto& before : DragBefore) {
                    if (!before.Actor || before.Actor->IsPendingKill())
                        continue;
                    glm::vec3 newScale = before.Scale;
                    if (ActiveAxis == EGizmoAxis::X)
                        newScale.x = std::max(0.01f, before.Scale.x * scaleFactor);
                    else if (ActiveAxis == EGizmoAxis::Y)
                        newScale.y = std::max(0.01f, before.Scale.y * scaleFactor);
                    else if (ActiveAxis == EGizmoAxis::Z)
                        newScale.z = std::max(0.01f, before.Scale.z * scaleFactor);

                    if (bSnapEnabled && ScaleSnap > 0.001f) {
                        newScale.x = std::round(newScale.x / ScaleSnap) * ScaleSnap;
                        newScale.y = std::round(newScale.y / ScaleSnap) * ScaleSnap;
                        newScale.z = std::round(newScale.z / ScaleSnap) * ScaleSnap;
                    }
                    before.Actor->SetActorScale(newScale);
                }
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ActiveAxis != EGizmoAxis::None) {
            CommitDragIfNeeded(InHistory);
            ActiveAxis = EGizmoAxis::None;
        }

        ImU32 colX = (ActiveAxis == EGizmoAxis::X || hoveredAxis == EGizmoAxis::X) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(235, 50, 60, 240);
        ImU32 colY = (ActiveAxis == EGizmoAxis::Y || hoveredAxis == EGizmoAxis::Y) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(50, 220, 70, 240);
        ImU32 colZ = (ActiveAxis == EGizmoAxis::Z || hoveredAxis == EGizmoAxis::Z) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(60, 130, 245, 240);

        float lineThick = 2.5f;

        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posXScreen.x, posXScreen.y), colX, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posXScreen.x, posXScreen.y), 4.5f, colX);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posXScreen.x - 3.5f, posXScreen.y - 3.5f),
                                    ImVec2(posXScreen.x + 3.5f, posXScreen.y + 3.5f), colX);
        }

        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posYScreen.x, posYScreen.y), colY, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posYScreen.x, posYScreen.y), 4.5f, colY);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posYScreen.x - 3.5f, posYScreen.y - 3.5f),
                                    ImVec2(posYScreen.x + 3.5f, posYScreen.y + 3.5f), colY);
        }

        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posZScreen.x, posZScreen.y), colZ, lineThick);
        if (CurrentOperation == EGizmoOperation::Translate) {
            drawList->AddCircleFilled(ImVec2(posZScreen.x, posZScreen.y), 4.5f, colZ);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posZScreen.x - 3.5f, posZScreen.y - 3.5f),
                                    ImVec2(posZScreen.x + 3.5f, posZScreen.y + 3.5f), colZ);
        }

        drawList->AddCircleFilled(ImVec2(originScreen.x, originScreen.y), 4.0f, IM_COL32(240, 240, 245, 255));
    }

} // namespace Leon::Editor
