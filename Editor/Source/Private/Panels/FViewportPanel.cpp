#include "Editor/Panels/FViewportPanel.hpp"
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
            AActor* primaryActor = Context ? Context->GetSelection().GetPrimarySelectedActor() : nullptr;
            if (primaryActor) {
                FocusOnActor(primaryActor);
            }
        }

        // Hotkey 'Delete' to delete selected actor
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput && Context) {
            AActor* primaryActor = Context->GetSelection().GetPrimarySelectedActor();
            if (primaryActor && Context->GetActiveWorld()) {
                Context->GetActiveWorld()->DestroyActor(primaryActor);
                Context->GetSelection().ClearActorSelection();
            }
        }

        // Camera Speed adjustment via mouse wheel
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
            CameraSpeed = std::clamp(CameraSpeed + io.MouseWheel * 1.5f, 1.0f, 50.0f);
        }

        // RMB Navigation (Unreal style: WASD + QE)
        if (ImGui::IsMouseDown(1) && ImGui::IsWindowHovered()) {
            float speed = CameraSpeed * (io.KeyShift ? 3.0f : 1.0f);
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
                float yaw = EditorCamera.GetYaw() + io.MouseDelta.x * sens;
                float pitch = std::clamp(EditorCamera.GetPitch() - io.MouseDelta.y * sens, -89.0f, 89.0f);
                EditorCamera.SetRotation(pitch, yaw);
            }
        }
    }

    void FViewportPanel::RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight) {
        if (InWidth == 0 || InHeight == 0)
            return;

        if (!WorldRenderer) {
            WorldRenderer = std::make_unique<FWorldRenderer>(&InWorld);
        } else {
            WorldRenderer->SetWorld(&InWorld);
        }

        WorldRenderer->OnViewportResize(InWidth, InHeight);

        float aspect = static_cast<float>(InWidth) / static_cast<float>(InHeight);
        EditorCamera.SetProjection(45.0f, aspect, 0.1f, 1000.0f);

        WorldRenderer->SetWireframeEnabled(ShadingMode == EViewportShadingMode::Wireframe);
        WorldRenderer->Render(EditorCamera);
    }

    void FViewportPanel::DrawViewportToolbar() {
        float btnWidth = 26.0f;
        float btnHeight = 24.0f;
        ImVec2 toolBtnSize(btnWidth, btnHeight);

        // Tool Selection: Q = Select, W = Translate, E = Rotate, R = Scale
        auto DrawToolBtn = [&](EGizmoOperation Op, const char* label, const char* tooltip) {
            bool bActive = (Gizmo.GetOperation() == Op);
            if (bActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.45f, 0.78f, 1.0f));
            }
            if (ImGui::Button(label, toolBtnSize)) {
                Gizmo.SetOperation(Op);
            }
            if (bActive) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::SameLine();
        };

        DrawToolBtn(EGizmoOperation::Select, "Q", "Select (Q)");
        DrawToolBtn(EGizmoOperation::Translate, "W", "Translate (W)");
        DrawToolBtn(EGizmoOperation::Rotate, "E", "Rotate (E)");
        DrawToolBtn(EGizmoOperation::Scale, "R", "Scale (R)");

        ImGui::Spacing();
        ImGui::SameLine();

        // Coordinate Space Toggle: World / Local
        bool bWorld = (Gizmo.GetMode() == EGizmoMode::World);
        if (ImGui::Button(bWorld ? "World" : "Local", ImVec2(50.0f, btnHeight))) {
            Gizmo.SetMode(bWorld ? EGizmoMode::Local : EGizmoMode::World);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Coordinate Mode: %s", bWorld ? "World Space" : "Local Space");
        }

        ImGui::SameLine();

        // Snapping Toggle
        bool bSnap = Gizmo.IsSnappingEnabled();
        if (bSnap) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.35f, 1.0f));
        }
        if (ImGui::Button("Snap", ImVec2(44.0f, btnHeight))) {
            Gizmo.SetSnappingEnabled(!bSnap);
        }
        if (bSnap) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grid Snapping: %s", bSnap ? "Enabled" : "Disabled");
        }

        ImGui::SameLine();

        // Snapping Value Dropdown
        if (ImGui::Button("##SnapMenu", ImVec2(18.0f, btnHeight))) {
            ImGui::OpenPopup("SnapSettingsPopup");
        }
        if (ImGui::BeginPopup("SnapSettingsPopup")) {
            ImGui::TextDisabled("Snap Settings");
            ImGui::Separator();

            ImGui::Text("Translation Snap");
            float tSnaps[] = {1.0f, 5.0f, 10.0f, 50.0f, 100.0f};
            for (float s : tSnaps) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.0f units", s);
                if (ImGui::MenuItem(lbl, nullptr, Gizmo.GetTranslationSnap() == s)) {
                    Gizmo.SetTranslationSnap(s);
                }
            }

            ImGui::Separator();
            ImGui::Text("Rotation Snap");
            float rSnaps[] = {5.0f, 15.0f, 45.0f, 90.0f};
            for (float s : rSnaps) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.0f deg", s);
                if (ImGui::MenuItem(lbl, nullptr, Gizmo.GetRotationSnap() == s)) {
                    Gizmo.SetRotationSnap(s);
                }
            }

            ImGui::Separator();
            ImGui::Text("Scale Snap");
            float sSnaps[] = {0.1f, 0.25f, 0.5f, 1.0f};
            for (float s : sSnaps) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.2f", s);
                if (ImGui::MenuItem(lbl, nullptr, Gizmo.GetScaleSnap() == s)) {
                    Gizmo.SetScaleSnap(s);
                }
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // View Mode: Perspective / Top / Front / Side
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

            // Active Primary Selected Actor
            AActor* activeActor = Context ? Context->GetSelection().GetPrimarySelectedActor() : InSelectedActor;

            // Draw 3D Interactive Transform Gizmo
            if (activeActor) {
                Gizmo.Draw(activeActor, EditorCamera, vpMin.x, vpMin.y, vpSize.x, vpSize.y);
            }

            // Draw Selection Wireframe
            if (activeActor) {
                DrawSelectionOutline(activeActor, vpMin, vpSize);
            }

            // Marquee Selection Box
            if (InWorld) {
                ProcessMarqueeSelection(*InWorld, vpMin, vpSize);
            }

            // Actor Selection and Raycast Picking
            if (InWorld && ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !Gizmo.IsDragging() && !Gizmo.IsHovered()) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.y > vpMin.y + 36.0f) { // Below toolbar
                    glm::vec2 clickPos(mousePos.x, mousePos.y);
                    AActor* hitActor = PickActorAtScreenPos(*InWorld, clickPos, vpMin, vpSize);

                    bool bCtrl = ImGui::GetIO().KeyCtrl;
                    bool bShift = ImGui::GetIO().KeyShift;

                    if (Context) {
                        if (hitActor) {
                            if (bCtrl) {
                                Context->GetSelection().ToggleActorSelection(hitActor);
                            } else if (bShift) {
                                Context->GetSelection().SelectActor(hitActor, true);
                            } else {
                                Context->GetSelection().SelectActor(hitActor, false);
                            }
                        } else if (!bCtrl && !bShift) {
                            Context->GetSelection().ClearActorSelection();
                        }
                    }

                    if (OnActorSelected) {
                        OnActorSelected(hitActor);
                    }
                }
            }

            // Drag and Drop Targets from Place Actors & Content Browser
            if (InWorld && ImGui::BeginDragDropTarget()) {
                // Actor Spawning from Place Actors
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLACE_ACTOR_TYPE")) {
                    const char* actorType = static_cast<const char*>(payload->Data);
                    if (actorType) {
                        glm::vec3 spawnPos = GetWorldRayIntersection(
                            glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y), vpMin, vpSize);
                        AActor* spawned = FPlaceActorsPanel::SpawnActorAt(*InWorld, actorType, spawnPos);
                        if (spawned) {
                            if (Context) {
                                Context->GetSelection().SelectActor(spawned, false);
                            }
                            if (OnActorSpawned) {
                                OnActorSpawned(spawned);
                            }
                        }
                    }
                }

                // Mesh Spawning from Content Browser
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    const char* assetPath = static_cast<const char*>(payload->Data);
                    if (assetPath) {
                        std::string pathStr(assetPath);
                        if (pathStr.ends_with(".obj") || pathStr.ends_with(".fbx") || pathStr.ends_with(".gltf")) {
                            glm::vec3 spawnPos = GetWorldRayIntersection(
                                glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y), vpMin, vpSize);
                            AActor* spawned = InWorld->SpawnActor<AActor>("DroppedMesh");
                            if (spawned) {
                                auto& tc = spawned->AddComponent<FTransformComponent>();
                                tc.Translation = spawnPos;
                                auto& sm = spawned->AddComponent<FStaticMeshComponent>();
                                sm.AssetPath = pathStr;
                                if (Context) {
                                    Context->GetSelection().SelectActor(spawned, false);
                                }
                                if (OnActorSpawned) {
                                    OnActorSpawned(spawned);
                                }
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            // Statistics Overlay
            if (bShowStatistics && InWorld) {
                DrawViewportOverlay(InMapName, activeActor, static_cast<uint32_t>(InWorld->GetAllActors().size()));
            }

        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Viewport Error: %s", e.what());
        }

        ImGui::End();
    }

    void FViewportPanel::ProcessMarqueeSelection(UWorld& InWorld, const ImVec2& InViewportMin,
                                                 const ImVec2& InViewportSize) {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !Gizmo.IsDragging()) {
            if (io.KeyShift || io.KeyCtrl) {
                bMarqueeSelecting = true;
                MarqueeStart = io.MousePos;
                MarqueeEnd = io.MousePos;
            }
        }

        if (bMarqueeSelecting) {
            MarqueeEnd = io.MousePos;

            // Draw translucent marquee box
            ImVec2 bMin(std::min(MarqueeStart.x, MarqueeEnd.x), std::min(MarqueeStart.y, MarqueeEnd.y));
            ImVec2 bMax(std::max(MarqueeStart.x, MarqueeEnd.x), std::max(MarqueeStart.y, MarqueeEnd.y));

            drawList->AddRectFilled(bMin, bMax, IM_COL32(50, 150, 255, 45));
            drawList->AddRect(bMin, bMax, IM_COL32(80, 180, 255, 200), 0.0f, 0, 1.5f);

            if (ImGui::IsMouseReleased(0)) {
                bMarqueeSelecting = false;

                glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();
                std::vector<AActor*> enclosedActors;

                for (const auto& actor : InWorld.GetAllActors()) {
                    if (!actor || !actor->template HasComponent<FTransformComponent>())
                        continue;

                    const auto& tc = actor->template GetComponent<FTransformComponent>();
                    glm::vec4 clip = vp * glm::vec4(tc.Translation, 1.0f);
                    if (clip.w <= 0.001f)
                        continue;

                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    ImVec2 screenPos(InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x,
                                     InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y);

                    if (screenPos.x >= bMin.x && screenPos.x <= bMax.x && screenPos.y >= bMin.y &&
                        screenPos.y <= bMax.y) {
                        enclosedActors.push_back(actor.get());
                    }
                }

                if (Context) {
                    if (io.KeyShift) {
                        for (AActor* a : enclosedActors) {
                            Context->GetSelection().SelectActor(a, true);
                        }
                    } else {
                        Context->GetSelection().SetSelectedActors(enclosedActors);
                    }
                }
            }
        }
    }

    AActor* FViewportPanel::PickActorAtScreenPos(UWorld& InWorld, const glm::vec2& InScreenPos,
                                                 const ImVec2& InViewportMin, const ImVec2& InViewportSize) {
        if (InViewportSize.x <= 0.0f || InViewportSize.y <= 0.0f)
            return nullptr;

        float relX = (InScreenPos.x - InViewportMin.x) / InViewportSize.x;
        float relY = (InScreenPos.y - InViewportMin.y) / InViewportSize.y;

        if (relX < 0.0f || relX > 1.0f || relY < 0.0f || relY > 1.0f)
            return nullptr;

        float ndcX = relX * 2.0f - 1.0f;
        float ndcY = 1.0f - relY * 2.0f;

        glm::mat4 invVP = glm::inverse(EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix());
        glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < 1e-6f || std::abs(farPoint.w) < 1e-6f)
            return nullptr;

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        glm::vec3 rayOrigin = glm::vec3(nearPoint);
        glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));

        AActor* closestActor = nullptr;
        float closestDist = 100000.0f;

        auto RayIntersectsAABB = [](const glm::vec3& rOrigin, const glm::vec3& rDir,
                                    const glm::vec3& boxMin, const glm::vec3& boxMax, float& outT) -> bool {
            float tMin = 0.0f;
            float tMax = 10000.0f;

            for (int i = 0; i < 3; ++i) {
                if (std::abs(rDir[i]) < 1e-6f) {
                    if (rOrigin[i] < boxMin[i] || rOrigin[i] > boxMax[i])
                        return false;
                } else {
                    float invD = 1.0f / rDir[i];
                    float t1 = (boxMin[i] - rOrigin[i]) * invD;
                    float t2 = (boxMax[i] - rOrigin[i]) * invD;
                    if (t1 > t2) std::swap(t1, t2);
                    tMin = std::max(tMin, t1);
                    tMax = std::min(tMax, t2);
                    if (tMin > tMax)
                        return false;
                }
            }
            outT = tMin;
            return true;
        };

        for (const auto& actor : InWorld.GetAllActors()) {
            if (!actor || !actor->template HasComponent<FTransformComponent>())
                continue;

            const auto& tc = actor->template GetComponent<FTransformComponent>();
            glm::mat4 worldTransform = tc.GetTransform();
            glm::mat4 invWorld = glm::inverse(worldTransform);

            // Transform ray into Actor's Local Coordinate Space
            glm::vec3 localRayOrigin = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
            glm::vec3 localRayDir = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));
            float localDirLen = glm::length(localRayDir);
            if (localDirLen < 1e-6f)
                continue;
            localRayDir /= localDirLen;

            glm::vec3 boxMin(-0.5f);
            glm::vec3 boxMax(0.5f);

            if (actor->template HasComponent<FBoxCollisionComponent>()) {
                const auto& col = actor->template GetComponent<FBoxCollisionComponent>();
                boxMin = col.LocalMin;
                boxMax = col.LocalMax;
            } else if (actor->template HasComponent<FStaticMeshComponent>()) {
                const auto& smc = actor->template GetComponent<FStaticMeshComponent>();
                if (smc.StaticMesh && glm::length(smc.StaticMesh->GetBoundsMax() - smc.StaticMesh->GetBoundsMin()) > 0.001f) {
                    boxMin = smc.StaticMesh->GetBoundsMin();
                    boxMax = smc.StaticMesh->GetBoundsMax();
                }
            } else if (actor->template HasComponent<FMeshComponent>()) {
                const auto& mc = actor->template GetComponent<FMeshComponent>();
                if (mc.MeshType == "Plane") {
                    boxMin = glm::vec3(-0.5f * mc.MeshSize, -0.05f, -0.5f * mc.MeshSize);
                    boxMax = glm::vec3(0.5f * mc.MeshSize, 0.05f, 0.5f * mc.MeshSize);
                } else {
                    boxMin = glm::vec3(-0.5f * mc.MeshSize);
                    boxMax = glm::vec3(0.5f * mc.MeshSize);
                }
            }

            // Expand thin boxes slightly for easier clicking in viewport
            for (int i = 0; i < 3; ++i) {
                if (boxMax[i] - boxMin[i] < 0.1f) {
                    float mid = (boxMin[i] + boxMax[i]) * 0.5f;
                    boxMin[i] = mid - 0.1f;
                    boxMax[i] = mid + 0.1f;
                }
            }

            float hitLocalT = 0.0f;
            if (RayIntersectsAABB(localRayOrigin, localRayDir, boxMin, boxMax, hitLocalT)) {
                glm::vec3 hitLocalPos = localRayOrigin + localRayDir * hitLocalT;
                glm::vec3 hitWorldPos = glm::vec3(worldTransform * glm::vec4(hitLocalPos, 1.0f));
                float hitWorldDist = glm::dot(hitWorldPos - rayOrigin, rayDir);

                if (hitWorldDist > 0.0f && hitWorldDist < closestDist) {
                    closestDist = hitWorldDist;
                    closestActor = actor.get();
                }
            }
        }

        // Secondary fallback: screen-distance test for actors without mesh volume (lights, empty actors, cameras)
        if (!closestActor) {
            float minScreenDist = 24.0f; // in pixels
            glm::mat4 viewProj = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

            for (const auto& actor : InWorld.GetAllActors()) {
                if (!actor || !actor->template HasComponent<FTransformComponent>())
                    continue;

                const auto& tc = actor->template GetComponent<FTransformComponent>();
                glm::vec4 clip = viewProj * glm::vec4(tc.Translation, 1.0f);
                if (clip.w > 0.001f) {
                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    float sx = InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x;
                    float sy = InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y;
                    float screenDist = glm::length(glm::vec2(sx, sy) - InScreenPos);

                    if (screenDist < minScreenDist) {
                        minScreenDist = screenDist;
                        closestActor = actor.get();
                    }
                }
            }
        }

        return closestActor;
    }

    glm::vec3 FViewportPanel::GetWorldRayIntersection(const glm::vec2& InScreenPos, const ImVec2& InViewportMin,
                                                      const ImVec2& InViewportSize) {
        if (InViewportSize.x <= 0.0f || InViewportSize.y <= 0.0f)
            return glm::vec3(0.0f);

        float relX = (InScreenPos.x - InViewportMin.x) / InViewportSize.x;
        float relY = (InScreenPos.y - InViewportMin.y) / InViewportSize.y;

        float ndcX = relX * 2.0f - 1.0f;
        float ndcY = 1.0f - relY * 2.0f;

        glm::mat4 invVP = glm::inverse(EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix());
        glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < 1e-6f || std::abs(farPoint.w) < 1e-6f)
            return glm::vec3(0.0f);

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
        size_t selCount = Context ? Context->GetSelection().GetSelectedActorCount() : (InSelectedActor ? 1 : 0);

        char statsText[256];
        snprintf(statsText, sizeof(statsText), "Map: %s  |  FPS: %.0f (%.1f ms)  |  Actors: %u  |  Selected: %zu (%s)",
                 InMapName.empty() ? "Untitled" : InMapName.c_str(), FrameRate, FrameTimeMs, InActorCount, selCount,
                 selName);

        drawList->AddText(ImVec2(overlayPos.x + 8.0f, overlayPos.y + 4.0f), IM_COL32(200, 205, 215, 255), statsText);
    }

} // namespace Leon::Editor
