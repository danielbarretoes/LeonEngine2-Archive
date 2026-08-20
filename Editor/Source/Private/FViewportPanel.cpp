#include "Editor/FViewportPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace Leon::Editor {

    void FViewportPanel::EnsureCamera() {
        if (!bCameraInitialized) {
            EditorCamera.SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));
            EditorCamera.SetRotation(-20.0f, -90.0f);
            bCameraInitialized = true;
        }
    }

    void FViewportPanel::FocusOnActor(AActor* InActor) {
        if (!InActor || !InActor->HasComponent<FTransformComponent>()) {
            return;
        }

        const auto& tc = InActor->GetComponent<FTransformComponent>();
        glm::vec3 target = tc.Translation;
        glm::vec3 forward = EditorCamera.GetForwardDirection();
        float dist = 6.0f;
        EditorCamera.SetPosition(target - forward * dist);
    }

    void FViewportPanel::ProcessCameraInput() {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        if (dt <= 0.0f || dt > 0.1f)
            dt = 0.016f;

        // Hotkey 'F' to focus selected actor
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !io.WantTextInput) {
            AActor* primaryActor = SelectionSubsystem ? SelectionSubsystem->GetPrimarySelectedActor() : nullptr;
            if (primaryActor) {
                FocusOnActor(primaryActor);
            }
        }

        // Camera Speed adjustment via mouse wheel
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
            CameraSpeed = std::clamp(CameraSpeed + io.MouseWheel * 1.5f, 1.0f, 50.0f);
        }

        // Unreal-style RMB Free-fly
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered()) {
            float speed = CameraSpeed;
            if (io.KeyShift)
                speed *= 2.5f;

            glm::vec3 moveDir(0.0f);
            if (ImGui::IsKeyDown(ImGuiKey_W))
                moveDir += EditorCamera.GetForwardDirection();
            if (ImGui::IsKeyDown(ImGuiKey_S))
                moveDir -= EditorCamera.GetForwardDirection();
            if (ImGui::IsKeyDown(ImGuiKey_D))
                moveDir += EditorCamera.GetRightDirection();
            if (ImGui::IsKeyDown(ImGuiKey_A))
                moveDir -= EditorCamera.GetRightDirection();
            if (ImGui::IsKeyDown(ImGuiKey_E))
                moveDir += EditorCamera.GetUpDirection();
            if (ImGui::IsKeyDown(ImGuiKey_Q))
                moveDir -= EditorCamera.GetUpDirection();

            if (glm::length(moveDir) > 0.001f) {
                EditorCamera.SetPosition(EditorCamera.GetPosition() + glm::normalize(moveDir) * speed * dt);
            }

            // Mouse Look
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                float sens = 0.15f;
                float newYaw = EditorCamera.GetYaw() + io.MouseDelta.x * sens;
                float newPitch = std::clamp(EditorCamera.GetPitch() - io.MouseDelta.y * sens, -89.0f, 89.0f);
                EditorCamera.SetRotation(newPitch, newYaw);
            }
        }
    }

    void FViewportPanel::RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight) {
        if (!WorldRenderer) {
            WorldRenderer = std::make_unique<FWorldRenderer>(&InWorld);
        }

        WorldRenderer->OnViewportResize(InWidth, InHeight);
        float aspect = static_cast<float>(InWidth) / static_cast<float>(std::max(1u, InHeight));
        EditorCamera.SetProjection(EditorCamera.GetFOV(), aspect, EditorCamera.GetNearClip(),
                                   EditorCamera.GetFarClip());

        WorldRenderer->SetWireframeEnabled(ShadingMode == EViewportShadingMode::Wireframe);
        WorldRenderer->Render(EditorCamera);
    }

    void FViewportPanel::DrawViewportToolbar() {
        // Transform Tool Buttons: Select (Q), Move (W), Rotate (E), Scale (R)
        EGizmoOperation currentOp = Gizmo.GetOperation();

        auto DrawToolBtn = [this, currentOp](const char* label, EGizmoOperation op, const char* tooltip) {
            bool bActive = (currentOp == op);
            if (bActive)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.8f, 1.0f));
            if (ImGui::Button(label, ImVec2(30.0f, 24.0f))) {
                Gizmo.SetOperation(op);
            }
            if (bActive)
                ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);
            ImGui::SameLine();
        };

        DrawToolBtn("Q##SelectTool", EGizmoOperation::Select, "Select (Q)");
        DrawToolBtn("W##MoveTool", EGizmoOperation::Translate, "Translate / Move (W)");
        DrawToolBtn("E##RotateTool", EGizmoOperation::Rotate, "Rotate (E)");
        DrawToolBtn("R##ScaleTool", EGizmoOperation::Scale, "Scale (R)");

        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Coordinate System: World / Local
        EGizmoMode currentMode = Gizmo.GetMode();
        if (ImGui::Button(currentMode == EGizmoMode::World ? "World" : "Local", ImVec2(52.0f, 24.0f))) {
            Gizmo.SetMode(currentMode == EGizmoMode::World ? EGizmoMode::Local : EGizmoMode::World);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle World / Local coordinate space");

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Snapping Controls
        bool bSnap = Gizmo.IsSnappingEnabled();
        if (bSnap)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.28f, 1.0f));
        if (ImGui::Button("Snap", ImVec2(48.0f, 24.0f))) {
            Gizmo.SetSnappingEnabled(!bSnap);
        }
        if (bSnap)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle Grid & Angle Snapping");

        ImGui::SameLine();

        // Snap value selector popup
        if (ImGui::Button("##SnapSettings", ImVec2(20.0f, 24.0f))) {
            ImGui::OpenPopup("SnapSettingsPopup");
        }
        if (ImGui::BeginPopup("SnapSettingsPopup")) {
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Snapping Increments");
            ImGui::Separator();

            float transSnap = Gizmo.GetTranslationSnap();
            if (ImGui::RadioButton("Move: 10 units", transSnap == 10.0f))
                Gizmo.SetTranslationSnap(10.0f);
            if (ImGui::RadioButton("Move: 50 units", transSnap == 50.0f))
                Gizmo.SetTranslationSnap(50.0f);
            if (ImGui::RadioButton("Move: 100 units", transSnap == 100.0f))
                Gizmo.SetTranslationSnap(100.0f);

            ImGui::Separator();
            float rotSnap = Gizmo.GetRotationSnap();
            if (ImGui::RadioButton("Rotate: 15 deg", rotSnap == 15.0f))
                Gizmo.SetRotationSnap(15.0f);
            if (ImGui::RadioButton("Rotate: 45 deg", rotSnap == 45.0f))
                Gizmo.SetRotationSnap(45.0f);
            if (ImGui::RadioButton("Rotate: 90 deg", rotSnap == 90.0f))
                Gizmo.SetRotationSnap(90.0f);

            ImGui::Separator();
            float scaleSnap = Gizmo.GetScaleSnap();
            if (ImGui::RadioButton("Scale: 0.25", scaleSnap == 0.25f))
                Gizmo.SetScaleSnap(0.25f);
            if (ImGui::RadioButton("Scale: 0.50", scaleSnap == 0.5f))
                Gizmo.SetScaleSnap(0.5f);
            if (ImGui::RadioButton("Scale: 1.00", scaleSnap == 1.0f))
                Gizmo.SetScaleSnap(1.0f);

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // View Mode: Perspective / Orthographic
        const char* viewModeNames[] = {"Perspective", "Top", "Front", "Side"};
        ImGui::SetNextItemWidth(100.0f);
        int vmIdx = static_cast<int>(ViewMode);
        if (ImGui::Combo("##ViewModeCombo", &vmIdx, viewModeNames, 4)) {
            ViewMode = static_cast<EViewportViewMode>(vmIdx);
        }

        ImGui::SameLine();

        // Shading Mode: Lit / Unlit / Wireframe
        const char* shadingNames[] = {"Lit", "Unlit", "Wireframe"};
        ImGui::SetNextItemWidth(90.0f);
        int smIdx = static_cast<int>(ShadingMode);
        if (ImGui::Combo("##ShadingCombo", &smIdx, shadingNames, 3)) {
            ShadingMode = static_cast<EViewportShadingMode>(smIdx);
        }

        ImGui::SameLine();

        // Stats Toggle
        if (ImGui::SmallButton(bShowStatistics ? "Hide Stats" : "Show Stats")) {
            bShowStatistics = !bShowStatistics;
        }
    }

    void FViewportPanel::Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor,
                              bool* bInOutOpen) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport", bInOutOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        try {
            EnsureCamera();
            ProcessCameraInput();

            // Toolbar at top of viewport
            ImGui::SetCursorPos(ImVec2(8.0f, 28.0f));
            DrawViewportToolbar();

            ImVec2 vMin = ImGui::GetWindowContentRegionMin();
            ImVec2 vMax = ImGui::GetWindowContentRegionMax();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 vpMin(winPos.x + vMin.x, winPos.y + vMin.y);
            ImVec2 vpSize(vMax.x - vMin.x, vMax.y - vMin.y);

            uint32_t w = std::max(1u, static_cast<uint32_t>(vpSize.x));
            uint32_t h = std::max(1u, static_cast<uint32_t>(vpSize.y));

            if (InWorld) {
                RenderWorld(*InWorld, w, h);

                if (WorldRenderer && WorldRenderer->GetHDRSceneFramebuffer()) {
                    uint32_t texId = WorldRenderer->GetHDRSceneFramebuffer()->GetColorAttachmentRendererID(0);
                    if (texId != 0) {
                        ImGui::SetCursorPos(vMin);
                        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(texId)), vpSize, ImVec2(0, 1),
                                     ImVec2(1, 0));
                    }
                }
            }

            // Marquee Selection Box
            if (InWorld) {
                ProcessMarqueeSelection(*InWorld, vpMin, vpSize);
            }

            // Actor Selection and Raycast Picking
            if (InWorld && ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !Gizmo.IsDragging()) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.y > vpMin.y + 36.0f) { // Below toolbar
                    glm::vec2 clickPos(mousePos.x, mousePos.y);
                    AActor* hitActor = PickActorAtScreenPos(*InWorld, clickPos, vpMin, vpSize);

                    bool bCtrl = ImGui::GetIO().KeyCtrl;
                    bool bShift = ImGui::GetIO().KeyShift;

                    if (SelectionSubsystem) {
                        if (hitActor) {
                            if (bCtrl) {
                                SelectionSubsystem->ToggleActorSelection(hitActor);
                            } else if (bShift) {
                                SelectionSubsystem->SelectActor(hitActor, true);
                            } else {
                                SelectionSubsystem->SelectActor(hitActor, false);
                            }
                        } else if (!bCtrl && !bShift) {
                            SelectionSubsystem->ClearActorSelection();
                        }
                    }

                    if (OnActorSelected) {
                        OnActorSelected(hitActor);
                    }
                }
            }

            // Active Primary Selected Actor
            AActor* activeActor = SelectionSubsystem ? SelectionSubsystem->GetPrimarySelectedActor() : InSelectedActor;

            // Draw Selection Wireframe
            if (activeActor) {
                DrawSelectionOutline(activeActor, vpMin, vpSize);
            }

            // Draw 3D Interactive Transform Gizmo
            if (activeActor) {
                Gizmo.Draw(activeActor, EditorCamera, vpMin.x, vpMin.y, vpSize.x, vpSize.y);
            }

            // Drag & Drop Targets
            if (ImGui::BeginDragDropTarget()) {
                ImVec2 mousePos = ImGui::GetMousePos();
                glm::vec2 screenPos(mousePos.x, mousePos.y);

                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
                    if (strcmp(payload->DataType, "CONTENT_BROWSER_ASSET") == 0) {
                        std::string assetPath = static_cast<const char*>(payload->Data);
                        bool bIsMaterial = (assetPath.find(".lmat") != std::string::npos);

                        if (bIsMaterial && InWorld) {
                            AActor* hoveredActor = PickActorAtScreenPos(*InWorld, screenPos, vpMin, vpSize);
                            if (hoveredActor) {
                                DrawSelectionOutline(hoveredActor, vpMin, vpSize);
                            }
                        } else {
                            glm::vec3 dropPos = GetWorldRayIntersection(screenPos, vpMin, vpSize);
                            DrawPlacementGhost(dropPos, vpMin, vpSize, "Mesh Asset");
                        }
                    } else if (strcmp(payload->DataType, "PLACE_ACTOR_TYPE") == 0) {
                        glm::vec3 dropPos = GetWorldRayIntersection(screenPos, vpMin, vpSize);
                        const char* typeName = static_cast<const char*>(payload->Data);
                        DrawPlacementGhost(dropPos, vpMin, vpSize, typeName);
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    std::string assetPath = static_cast<const char*>(payload->Data);
                    bool bIsMaterial = (assetPath.find(".lmat") != std::string::npos);

                    if (bIsMaterial && InWorld) {
                        AActor* hitActor = PickActorAtScreenPos(*InWorld, screenPos, vpMin, vpSize);
                        if (hitActor) {
                            if (!hitActor->HasComponent<FMaterialComponent>()) {
                                hitActor->AddComponent<FMaterialComponent>();
                            }
                            hitActor->GetComponent<FMaterialComponent>().AssetPath = assetPath;
                        }
                    } else if (InWorld) {
                        glm::vec3 dropPos = GetWorldRayIntersection(screenPos, vpMin, vpSize);
                        AActor* spawned = FPlaceActorsPanel::SpawnActorAt(*InWorld, "Cube", dropPos);
                        if (spawned) {
                            if (spawned->HasComponent<FStaticMeshComponent>()) {
                                spawned->GetComponent<FStaticMeshComponent>().AssetPath = assetPath;
                            }
                            if (OnActorSpawned)
                                OnActorSpawned(spawned);
                        }
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLACE_ACTOR_TYPE")) {
                    if (InWorld) {
                        std::string typeName = static_cast<const char*>(payload->Data);
                        glm::vec3 dropPos = GetWorldRayIntersection(screenPos, vpMin, vpSize);
                        AActor* spawned = FPlaceActorsPanel::SpawnActorAt(*InWorld, typeName, dropPos);
                        if (spawned && OnActorSpawned) {
                            OnActorSpawned(spawned);
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            // Viewport Statistics & Info Overlay
            if (bShowStatistics && InWorld) {
                DrawViewportOverlay(InMapName, activeActor, static_cast<uint32_t>(InWorld->GetAllActors().size()));
            }

        } catch (const std::exception& e) {
            LE_CORE_ERROR("FViewportPanel: Exception during Draw: {0}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Viewport Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FViewportPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Viewport: Unknown error encountered");
        }

        ImGui::End();
    }

    void FViewportPanel::ProcessMarqueeSelection(UWorld& InWorld, const ImVec2& InViewportMin,
                                                 const ImVec2& InViewportSize) {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;

        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !Gizmo.IsDragging()) {
            if (mousePos.y > InViewportMin.y + 36.0f) {
                bMarqueeSelecting = true;
                MarqueeStart = mousePos;
                MarqueeEnd = mousePos;
            }
        }

        if (bMarqueeSelecting) {
            if (ImGui::IsMouseDown(0)) {
                MarqueeEnd = mousePos;

                float minX = std::min(MarqueeStart.x, MarqueeEnd.x);
                float maxX = std::max(MarqueeStart.x, MarqueeEnd.x);
                float minY = std::min(MarqueeStart.y, MarqueeEnd.y);
                float maxY = std::max(MarqueeStart.y, MarqueeEnd.y);

                if (maxX - minX > 6.0f || maxY - minY > 6.0f) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32(30, 140, 255, 45));
                    drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), IM_COL32(60, 180, 255, 220), 0.0f, 0,
                                      1.5f);
                }
            } else if (ImGui::IsMouseReleased(0)) {
                float minX = std::min(MarqueeStart.x, MarqueeEnd.x);
                float maxX = std::max(MarqueeStart.x, MarqueeEnd.x);
                float minY = std::min(MarqueeStart.y, MarqueeEnd.y);
                float maxY = std::max(MarqueeStart.y, MarqueeEnd.y);

                if (maxX - minX > 8.0f && maxY - minY > 8.0f) {
                    glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();
                    std::vector<AActor*> selectedActors;

                    for (const auto& actorPtr : InWorld.GetAllActors()) {
                        AActor* actor = actorPtr.get();
                        if (!actor || !actor->HasComponent<FTransformComponent>())
                            continue;

                        glm::vec3 pos = actor->GetComponent<FTransformComponent>().Translation;
                        glm::vec4 clip = vp * glm::vec4(pos, 1.0f);
                        if (clip.w > 0.001f) {
                            glm::vec3 ndc = glm::vec3(clip) / clip.w;
                            float sx = InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x;
                            float sy = InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y;

                            if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY) {
                                selectedActors.push_back(actor);
                            }
                        }
                    }

                    if (!selectedActors.empty() && SelectionSubsystem) {
                        SelectionSubsystem->SetSelectedActors(selectedActors);
                    }
                }
                bMarqueeSelecting = false;
            }
        }
    }

    AActor* FViewportPanel::PickActorAtScreenPos(UWorld& InWorld, const glm::vec2& InScreenPos,
                                                 const ImVec2& InViewportMin, const ImVec2& InViewportSize) {
        float relX = (InScreenPos.x - InViewportMin.x) / InViewportSize.x;
        float relY = (InScreenPos.y - InViewportMin.y) / InViewportSize.y;
        if (relX < 0.0f || relX > 1.0f || relY < 0.0f || relY > 1.0f)
            return nullptr;

        float ndcX = relX * 2.0f - 1.0f;
        float ndcY = 1.0f - relY * 2.0f;

        glm::mat4 invVP = glm::inverse(EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix());
        glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        glm::vec3 rayOrigin = glm::vec3(nearPoint);
        glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));

        AActor* closestActor = nullptr;
        float closestDist = 1e9f;

        for (const auto& actorPtr : InWorld.GetAllActors()) {
            AActor* actor = actorPtr.get();
            if (!actor || !actor->HasComponent<FTransformComponent>())
                continue;

            const auto& tc = actor->GetComponent<FTransformComponent>();
            glm::vec3 toActor = tc.Translation - rayOrigin;
            float proj = glm::dot(toActor, rayDir);
            if (proj < 0.0f)
                continue;

            glm::vec3 perp = toActor - rayDir * proj;
            float radius = std::max(0.5f, std::max(tc.Scale.x, std::max(tc.Scale.y, tc.Scale.z)) * 0.8f);

            if (glm::length(perp) < radius && proj < closestDist) {
                closestDist = proj;
                closestActor = actor;
            }
        }

        return closestActor;
    }

    glm::vec3 FViewportPanel::GetWorldRayIntersection(const glm::vec2& InScreenPos, const ImVec2& InViewportMin,
                                                      const ImVec2& InViewportSize) {
        float relX = (InScreenPos.x - InViewportMin.x) / InViewportSize.x;
        float relY = (InScreenPos.y - InViewportMin.y) / InViewportSize.y;
        float ndcX = relX * 2.0f - 1.0f;
        float ndcY = 1.0f - relY * 2.0f;

        glm::mat4 invVP = glm::inverse(EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix());
        glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        glm::vec3 rayOrigin = glm::vec3(nearPoint);
        glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));

        if (std::abs(rayDir.y) > 0.0001f) {
            float t = -rayOrigin.y / rayDir.y;
            if (t > 0.0f)
                return rayOrigin + rayDir * t;
        }

        return rayOrigin + rayDir * 10.0f;
    }

    void FViewportPanel::DrawSelectionOutline(AActor* InSelectedActor, const ImVec2& InViewportMin,
                                              const ImVec2& InViewportSize) {
        if (!InSelectedActor || !InSelectedActor->HasComponent<FTransformComponent>())
            return;

        const auto& tc = InSelectedActor->GetComponent<FTransformComponent>();
        glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

        auto Project3D = [&](const glm::vec3& p, ImVec2& out) -> bool {
            glm::vec4 clip = vp * glm::vec4(p, 1.0f);
            if (clip.w <= 0.001f)
                return false;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            out = ImVec2(InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x,
                         InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y);
            return true;
        };

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 outlineCol = IM_COL32(255, 175, 20, 240);

        // Bounding wireframe box
        glm::vec3 hs = tc.Scale * 0.5f;
        glm::vec3 corners[8] = {
            tc.Translation + glm::vec3(-hs.x, -hs.y, -hs.z), tc.Translation + glm::vec3(hs.x, -hs.y, -hs.z),
            tc.Translation + glm::vec3(hs.x, hs.y, -hs.z),   tc.Translation + glm::vec3(-hs.x, hs.y, -hs.z),
            tc.Translation + glm::vec3(-hs.x, -hs.y, hs.z),  tc.Translation + glm::vec3(hs.x, -hs.y, hs.z),
            tc.Translation + glm::vec3(hs.x, hs.y, hs.z),    tc.Translation + glm::vec3(-hs.x, hs.y, hs.z)};

        ImVec2 p[8];
        bool v[8];
        for (int i = 0; i < 8; ++i)
            v[i] = Project3D(corners[i], p[i]);

        int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

        for (int i = 0; i < 12; ++i) {
            int a = edges[i][0], b = edges[i][1];
            if (v[a] && v[b]) {
                drawList->AddLine(p[a], p[b], outlineCol, 1.6f);
            }
        }
    }

    void FViewportPanel::DrawPlacementGhost(const glm::vec3& InWorldPos, const ImVec2& InViewportMin,
                                            const ImVec2& InViewportSize, const char* InLabel) {
        glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

        auto Project3D = [&](const glm::vec3& p, ImVec2& out) -> bool {
            glm::vec4 clip = vp * glm::vec4(p, 1.0f);
            if (clip.w <= 0.001f)
                return false;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            out = ImVec2(InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x,
                         InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y);
            return true;
        };

        ImVec2 center;
        if (!Project3D(InWorldPos, center))
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircle(center, 18.0f, IM_COL32(40, 220, 100, 240), 24, 2.0f);
        drawList->AddCircleFilled(center, 4.0f, IM_COL32(40, 220, 100, 255));
        drawList->AddText(ImVec2(center.x + 12.0f, center.y - 8.0f), IM_COL32(240, 240, 255, 255), InLabel);
    }

    void FViewportPanel::DrawViewportOverlay(const std::string& InMapName, AActor* InSelectedActor,
                                             uint32_t InActorCount) {
        ImGuiIO& io = ImGui::GetIO();
        FrameRate = io.Framerate;
        FrameTimeMs = (FrameRate > 0.0f) ? (1000.0f / FrameRate) : 16.6f;

        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 overlayPos(winPos.x + 10.0f, winPos.y + winSize.y - 32.0f);
        ImVec2 overlaySize(winSize.x - 20.0f, 22.0f);

        drawList->AddRectFilled(overlayPos, ImVec2(overlayPos.x + overlaySize.x, overlayPos.y + overlaySize.y),
                                IM_COL32(14, 15, 18, 190), 4.0f);

        const char* selName = InSelectedActor ? InSelectedActor->GetName().c_str() : "None";
        size_t selCount = SelectionSubsystem ? SelectionSubsystem->GetSelectedActorCount() : (InSelectedActor ? 1 : 0);

        char statsText[256];
        snprintf(statsText, sizeof(statsText), "Map: %s  |  FPS: %.0f (%.1f ms)  |  Actors: %u  |  Selected: %zu (%s)",
                 InMapName.empty() ? "Untitled" : InMapName.c_str(), FrameRate, FrameTimeMs, InActorCount, selCount,
                 selName);

        drawList->AddText(ImVec2(overlayPos.x + 8.0f, overlayPos.y + 4.0f), IM_COL32(200, 205, 215, 255), statsText);
    }

} // namespace Leon::Editor
