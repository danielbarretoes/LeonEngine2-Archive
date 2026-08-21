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

        const glm::mat4 invVP = glm::inverse(InCamera.GetViewProjectionMatrix());
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

        const bool bRmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ActiveAxis != EGizmoAxis::None) {
            CommitDragIfNeeded(InHistory);
            ActiveAxis = EGizmoAxis::None;
        }

        if (CurrentOperation == EGizmoOperation::Select) {
            if (ActiveAxis == EGizmoAxis::None)
                bHovered = false;
            return;
        }

        auto& tc = primary->GetComponent<FTransformComponent>();
        glm::vec3 actorPos = glm::vec3(primary->GetActorWorldMatrix()[3]);
        glm::mat4 viewProj = InCamera.GetViewProjectionMatrix();

        bool bOriginInFront = false;
        glm::vec2 originScreen =
            WorldToScreen(actorPos, viewProj, InViewportX, InViewportY, InViewportW, InViewportH, bOriginInFront);
        if (!bOriginInFront && ActiveAxis == EGizmoAxis::None) {
            bHovered = false;
            return;
        }

        float distToCam = glm::length(InCamera.GetPosition() - actorPos);
        float axisLength = std::max(0.6f, distToCam * 0.15f);

        glm::vec3 dirX = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 dirY = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 dirZ = glm::vec3(0.0f, 0.0f, 1.0f);

        if (CurrentMode == EGizmoMode::Local) {
            const glm::mat4 world = primary->GetActorWorldMatrix();
            dirX = glm::vec3(world[0]);
            dirY = glm::vec3(world[1]);
            dirZ = glm::vec3(world[2]);
            const float lx = glm::length(dirX);
            const float ly = glm::length(dirY);
            const float lz = glm::length(dirZ);
            dirX = lx > 1e-6f ? dirX / lx : glm::vec3(1.0f, 0.0f, 0.0f);
            dirY = ly > 1e-6f ? dirY / ly : glm::vec3(0.0f, 1.0f, 0.0f);
            dirZ = lz > 1e-6f ? dirZ / lz : glm::vec3(0.0f, 0.0f, 1.0f);
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

        bHovered = (hoveredAxis != EGizmoAxis::None || ActiveAxis != EGizmoAxis::None) && !bRmb;

        if (!bRmb && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredAxis != EGizmoAxis::None &&
            ActiveAxis == EGizmoAxis::None) {
            ActiveAxis = hoveredAxis;
            DragStartMouse = mouse;
            DragOriginScreen = originScreen;
            DragStartRadius = std::max(glm::length(mouse - originScreen), 4.0f);
            InitialActorLocation = actorPos;
            InitialActorRotation = tc.Rotation;
            InitialActorScale = tc.Scale;

            DragAxisOrigin = actorPos;
            if (ActiveAxis == EGizmoAxis::X)
                DragAxisDir = dirX;
            else if (ActiveAxis == EGizmoAxis::Y)
                DragAxisDir = dirY;
            else
                DragAxisDir = dirZ;
            const float axisLen = glm::length(DragAxisDir);
            DragAxisDir = axisLen > 1e-6f ? DragAxisDir / axisLen : glm::vec3(1.0f, 0.0f, 0.0f);

            glm::vec3 toCam = InCamera.GetPosition() - DragAxisOrigin;
            glm::vec3 planeSide = glm::cross(DragAxisDir, toCam);
            if (glm::length(planeSide) < 1e-4f)
                planeSide = glm::cross(DragAxisDir, InCamera.GetRightDirection());
            if (glm::length(planeSide) < 1e-4f)
                planeSide = glm::cross(DragAxisDir, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(planeSide) < 1e-4f)
                planeSide = glm::vec3(0.0f, 0.0f, 1.0f);
            DragPlaneNormal = glm::normalize(glm::cross(DragAxisDir, planeSide));

            DragStartAxisT = 0.0f;
            glm::vec3 rayOrigin, rayDir, hit;
            if (ScreenToWorldRay(InCamera, mouse, InViewportX, InViewportY, InViewportW, InViewportH, rayOrigin,
                                 rayDir) &&
                IntersectRayWithAxis(rayOrigin, rayDir, hit)) {
                DragStartAxisT = glm::dot(hit - DragAxisOrigin, DragAxisDir);
            }

            DragBefore.clear();
            for (AActor* actor : InActors) {
                if (!actor || actor->IsPendingKill() || !actor->HasComponent<FTransformComponent>())
                    continue;
                DragBefore.push_back(FTransformActorsCommand::Capture(actor));
            }
            bDragRecorded = !DragBefore.empty();
        }

        if (!bRmb && ImGui::IsMouseDown(ImGuiMouseButton_Left) && ActiveAxis != EGizmoAxis::None) {
            if (CurrentOperation == EGizmoOperation::Translate) {
                glm::vec3 rayOrigin, rayDir, hit;
                if (ScreenToWorldRay(InCamera, mouse, InViewportX, InViewportY, InViewportW, InViewportH, rayOrigin,
                                     rayDir) &&
                    IntersectRayWithAxis(rayOrigin, rayDir, hit)) {
                    const float axisT = glm::dot(hit - DragAxisOrigin, DragAxisDir);
                    const glm::vec3 deltaWorld = DragAxisDir * (axisT - DragStartAxisT);

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
                }

            } else if (CurrentOperation == EGizmoOperation::Rotate) {
                glm::vec2 a = DragStartMouse - DragOriginScreen;
                glm::vec2 b = mouse - DragOriginScreen;
                const float ang0 = std::atan2(a.y, a.x);
                const float ang1 = std::atan2(b.y, b.x);
                float rotAngle = glm::degrees(ang1 - ang0);
                if (rotAngle > 180.0f)
                    rotAngle -= 360.0f;
                if (rotAngle < -180.0f)
                    rotAngle += 360.0f;

                glm::vec3 rotDelta(0.0f);
                if (ActiveAxis == EGizmoAxis::X)
                    rotDelta.x = rotAngle;
                else if (ActiveAxis == EGizmoAxis::Y)
                    rotDelta.y = rotAngle;
                else
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
                const float d1 = glm::length(mouse - DragOriginScreen);
                float scaleFactor = d1 / DragStartRadius;
                scaleFactor = std::clamp(scaleFactor, 0.01f, 100.0f);

                for (const auto& before : DragBefore) {
                    if (!before.Actor || before.Actor->IsPendingKill())
                        continue;
                    glm::vec3 newScale = before.Scale;
                    if (ActiveAxis == EGizmoAxis::X)
                        newScale.x = std::max(0.01f, before.Scale.x * scaleFactor);
                    else if (ActiveAxis == EGizmoAxis::Y)
                        newScale.y = std::max(0.01f, before.Scale.y * scaleFactor);
                    else
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

        if (!bOriginInFront)
            return;

        ImU32 colX = (ActiveAxis == EGizmoAxis::X || hoveredAxis == EGizmoAxis::X) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(235, 50, 60, 240);
        ImU32 colY = (ActiveAxis == EGizmoAxis::Y || hoveredAxis == EGizmoAxis::Y) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(50, 220, 70, 240);
        ImU32 colZ = (ActiveAxis == EGizmoAxis::Z || hoveredAxis == EGizmoAxis::Z) ? IM_COL32(255, 240, 60, 255)
                                                                                   : IM_COL32(60, 130, 245, 240);

        auto DrawArrowHead = [&](glm::vec2 from, glm::vec2 to, ImU32 col) {
            glm::vec2 dir = to - from;
            float len = glm::length(dir);
            if (len < 1.0f)
                return;
            dir /= len;
            glm::vec2 n(-dir.y, dir.x);
            ImVec2 p0(to.x, to.y);
            ImVec2 p1(to.x - dir.x * 10.0f + n.x * 4.5f, to.y - dir.y * 10.0f + n.y * 4.5f);
            ImVec2 p2(to.x - dir.x * 10.0f - n.x * 4.5f, to.y - dir.y * 10.0f - n.y * 4.5f);
            drawList->AddTriangleFilled(p0, p1, p2, col);
        };

        const float lineThick = 2.5f;
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posXScreen.x, posXScreen.y), colX, lineThick);
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posYScreen.x, posYScreen.y), colY, lineThick);
        drawList->AddLine(ImVec2(originScreen.x, originScreen.y), ImVec2(posZScreen.x, posZScreen.y), colZ, lineThick);

        if (CurrentOperation == EGizmoOperation::Translate) {
            DrawArrowHead(originScreen, posXScreen, colX);
            DrawArrowHead(originScreen, posYScreen, colY);
            DrawArrowHead(originScreen, posZScreen, colZ);
        } else if (CurrentOperation == EGizmoOperation::Scale) {
            drawList->AddRectFilled(ImVec2(posXScreen.x - 4.0f, posXScreen.y - 4.0f),
                                    ImVec2(posXScreen.x + 4.0f, posXScreen.y + 4.0f), colX);
            drawList->AddRectFilled(ImVec2(posYScreen.x - 4.0f, posYScreen.y - 4.0f),
                                    ImVec2(posYScreen.x + 4.0f, posYScreen.y + 4.0f), colY);
            drawList->AddRectFilled(ImVec2(posZScreen.x - 4.0f, posZScreen.y - 4.0f),
                                    ImVec2(posZScreen.x + 4.0f, posZScreen.y + 4.0f), colZ);
        } else if (CurrentOperation == EGizmoOperation::Rotate) {
            const float radius = glm::length(posXScreen - originScreen);
            if (radius > 4.0f)
                drawList->AddCircle(ImVec2(originScreen.x, originScreen.y), radius, IM_COL32(200, 200, 210, 70), 48,
                                    1.5f);
            drawList->AddCircleFilled(ImVec2(posXScreen.x, posXScreen.y), 4.5f, colX);
            drawList->AddCircleFilled(ImVec2(posYScreen.x, posYScreen.y), 4.5f, colY);
            drawList->AddCircleFilled(ImVec2(posZScreen.x, posZScreen.y), 4.5f, colZ);
        }

        drawList->AddCircleFilled(ImVec2(originScreen.x, originScreen.y), 4.0f, IM_COL32(240, 240, 245, 255));
    }

} // namespace Leon::Editor
