#include "Editor/Panels/FViewportPanel.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"
#include "Editor/Context/FEditorHistory.hpp"
#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "RHI/FFramebuffer.hpp"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <imgui.h>
#include <vector>

namespace Leon::Editor {

    namespace {

        void TransformAABBCorners(const glm::vec3& InLocalMin, const glm::vec3& InLocalMax, const glm::mat4& InWorld,
                                  glm::vec3& OutMin, glm::vec3& OutMax) {
            OutMin = glm::vec3(std::numeric_limits<float>::max());
            OutMax = glm::vec3(-std::numeric_limits<float>::max());
            const glm::vec3 corners[8] = {
                {InLocalMin.x, InLocalMin.y, InLocalMin.z}, {InLocalMax.x, InLocalMin.y, InLocalMin.z},
                {InLocalMin.x, InLocalMax.y, InLocalMin.z}, {InLocalMax.x, InLocalMax.y, InLocalMin.z},
                {InLocalMin.x, InLocalMin.y, InLocalMax.z}, {InLocalMax.x, InLocalMin.y, InLocalMax.z},
                {InLocalMin.x, InLocalMax.y, InLocalMax.z}, {InLocalMax.x, InLocalMax.y, InLocalMax.z},
            };
            for (const glm::vec3& c : corners) {
                glm::vec3 w = glm::vec3(InWorld * glm::vec4(c, 1.0f));
                OutMin = glm::min(OutMin, w);
                OutMax = glm::max(OutMax, w);
            }
        }

        void ExpandThinAABB(glm::vec3& InOutMin, glm::vec3& InOutMax, float InMinExtent = 0.1f) {
            for (int i = 0; i < 3; ++i) {
                if (InOutMax[i] - InOutMin[i] < InMinExtent) {
                    float mid = (InOutMin[i] + InOutMax[i]) * 0.5f;
                    InOutMin[i] = mid - InMinExtent * 0.5f;
                    InOutMax[i] = mid + InMinExtent * 0.5f;
                }
            }
        }

    } // namespace

    bool FViewportPanel::GetActorEditorLocalBounds(AActor& InActor, glm::vec3& OutMin, glm::vec3& OutMax) {
        OutMin = glm::vec3(-0.5f);
        OutMax = glm::vec3(0.5f);
        bool bFound = false;

        // Prefer rendered mesh bounds (what the user clicks), matching FCollisionQuery for procedurals.
        if (InActor.HasComponent<FStaticMeshComponent>()) {
            const auto& smc = InActor.GetComponent<FStaticMeshComponent>();
            TRef<UStaticMesh> mesh = smc.StaticMesh;
            if (!mesh && !smc.AssetPath.empty()) {
                mesh = UAssetManager::GetStaticMesh(smc.AssetPath);
            }
            if (mesh) {
                const glm::vec3 bMin = mesh->GetBoundsMin();
                const glm::vec3 bMax = mesh->GetBoundsMax();
                if (glm::length(bMax - bMin) > 0.001f) {
                    OutMin = bMin;
                    OutMax = bMax;
                    bFound = true;
                }
            }
        }

        if (!bFound && InActor.HasComponent<FMeshComponent>()) {
            const auto& mesh = InActor.GetComponent<FMeshComponent>();
            if (mesh.MeshType == "Cube") {
                const float h = mesh.MeshSize * 0.5f;
                OutMin = glm::vec3(-h);
                OutMax = glm::vec3(h);
            } else if (mesh.MeshType == "Plane") {
                OutMin = glm::vec3(-mesh.MeshWidth * 0.5f, -0.05f, -mesh.MeshDepth * 0.5f);
                OutMax = glm::vec3(mesh.MeshWidth * 0.5f, 0.05f, mesh.MeshDepth * 0.5f);
            } else if (mesh.MeshType == "Sphere") {
                OutMin = glm::vec3(-mesh.MeshRadius);
                OutMax = glm::vec3(mesh.MeshRadius);
            } else if (mesh.MeshType == "Cylinder" || mesh.MeshType == "Cone") {
                OutMin = glm::vec3(-mesh.MeshRadius, -mesh.MeshHeight * 0.5f, -mesh.MeshRadius);
                OutMax = glm::vec3(mesh.MeshRadius, mesh.MeshHeight * 0.5f, mesh.MeshRadius);
            } else if (mesh.MeshType == "Quad") {
                OutMin = glm::vec3(-mesh.MeshWidth * 0.5f, -mesh.MeshHeight * 0.5f, -0.05f);
                OutMax = glm::vec3(mesh.MeshWidth * 0.5f, mesh.MeshHeight * 0.5f, 0.05f);
            } else {
                // Box, Ramp, Pyramid, and other dimensioned primitives
                const float hx = std::max(mesh.MeshWidth, mesh.MeshSize) * 0.5f;
                const float hy = std::max(mesh.MeshHeight, mesh.MeshSize) * 0.5f;
                const float hz = std::max(mesh.MeshDepth, mesh.MeshSize) * 0.5f;
                OutMin = glm::vec3(-hx, -hy, -hz);
                OutMax = glm::vec3(hx, hy, hz);
            }
            bFound = true;
        }

        if (!bFound && InActor.HasComponent<FSkinnedMeshRenderState>()) {
            const auto& sk = InActor.GetComponent<FSkinnedMeshRenderState>();
            if (sk.SkeletalMesh) {
                glm::vec3 bMin = sk.SkeletalMesh->GetBoundsMin();
                glm::vec3 bMax = sk.SkeletalMesh->GetBoundsMax();
                glm::mat4 relative = glm::translate(glm::mat4(1.0f), sk.RelativeLocation) *
                                     glm::toMat4(glm::quat(glm::radians(sk.RelativeRotation))) *
                                     glm::scale(glm::mat4(1.0f), sk.RelativeScale);
                TransformAABBCorners(bMin, bMax, relative, OutMin, OutMax);
                bFound = true;
            }
        }

        if (!bFound && InActor.HasComponent<FBoxCollisionComponent>()) {
            const auto& col = InActor.GetComponent<FBoxCollisionComponent>();
            OutMin = col.LocalMin;
            OutMax = col.LocalMax;
            bFound = true;
        }

        ExpandThinAABB(OutMin, OutMax, 0.1f);
        return bFound;
    }

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
        glm::vec3 localMin, localMax;
        GetActorEditorLocalBounds(*InActor, localMin, localMax);

        glm::vec3 worldMin, worldMax;
        TransformAABBCorners(localMin, localMax, tc.GetTransform(), worldMin, worldMax);
        const glm::vec3 target = (worldMin + worldMax) * 0.5f;
        const float radius = glm::length(worldMax - worldMin) * 0.5f;
        const float dist = std::max(radius * 2.5f, 2.0f);

        glm::vec3 forward = EditorCamera.GetForwardDirection();
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

        // Hotkey 'Delete' — handled by FEditorApp::DeleteSelectedActors (undoable).
        // Do not destroy here or Ctrl+Z cannot restore.

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

        if (bShowEditorGizmos) {
            DrawEditorWorldGizmos(InWorld);
        }
    }

    void FViewportPanel::DrawEditorWorldGizmos(UWorld& InWorld) {
        if (!WorldRenderer || !WorldRenderer->GetHDRSceneFramebuffer())
            return;

        auto fbo = WorldRenderer->GetHDRSceneFramebuffer();
        fbo->Bind();

        FDebugRenderer::BeginScene(EditorCamera);
        auto& reg = InWorld.GetRegistry();

        auto dirView = reg.view<FDirectionalLightComponent, FTransformComponent>();
        for (auto entity : dirView) {
            auto [dirComp, transform] = dirView.get<FDirectionalLightComponent, FTransformComponent>(entity);
            if (!dirComp.bEnabled)
                continue;
            FDirectionalLight light = dirComp.Light;
            if (glm::length(light.Direction) < 1e-5f)
                light.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
            FDebugRenderer::DrawDirectionalLightGizmo(light, transform.Translation, 2.5f);
        }

        auto pointView = reg.view<FPointLightComponent, FTransformComponent>();
        for (auto entity : pointView) {
            auto [pointComp, transform] = pointView.get<FPointLightComponent, FTransformComponent>(entity);
            if (!pointComp.bEnabled)
                continue;
            FPointLight light = pointComp.Light;
            light.Position = transform.Translation;
            FDebugRenderer::DrawPointLightGizmo(light);
        }

        auto spotView = reg.view<FSpotLightComponent, FTransformComponent>();
        for (auto entity : spotView) {
            auto [spotComp, transform] = spotView.get<FSpotLightComponent, FTransformComponent>(entity);
            if (!spotComp.bEnabled)
                continue;
            FSpotLight light = spotComp.Light;
            light.Position = transform.Translation;
            if (glm::length(light.Direction) < 1e-5f) {
                glm::mat4 rot = glm::toMat4(glm::quat(glm::radians(transform.Rotation)));
                light.Direction = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));
            }
            FDebugRenderer::DrawSpotLightGizmo(light);
        }

            // PlayerStart: diamond + forward arrow (Unreal-like spawn marker)
        for (const auto& actorRef : InWorld.GetAllActors()) {
            AActor* actor = actorRef.get();
            if (!actor || actor->IsPendingKill() || !actor->HasComponent<FTransformComponent>())
                continue;
            const bool bPlayerStart = dynamic_cast<APlayerStart*>(actor) != nullptr ||
                                      actor->GetClass().find("PlayerStart") != std::string::npos ||
                                      actor->GetName().find("PlayerStart") != std::string::npos;
            if (!bPlayerStart)
                continue;

            const auto& tc = actor->GetComponent<FTransformComponent>();
            const glm::vec3 p = tc.Translation;
            const glm::vec4 col(0.15f, 0.85f, 1.0f, 1.0f);
            const float s = 0.35f;
            FDebugRenderer::DrawLine(p + glm::vec3(0, s, 0), p + glm::vec3(s, 0, 0), col);
            FDebugRenderer::DrawLine(p + glm::vec3(s, 0, 0), p + glm::vec3(0, -s, 0), col);
            FDebugRenderer::DrawLine(p + glm::vec3(0, -s, 0), p + glm::vec3(-s, 0, 0), col);
            FDebugRenderer::DrawLine(p + glm::vec3(-s, 0, 0), p + glm::vec3(0, s, 0), col);
            FDebugRenderer::DrawLine(p + glm::vec3(0, s, 0), p + glm::vec3(0, 0, s), col);
            FDebugRenderer::DrawLine(p + glm::vec3(0, 0, s), p + glm::vec3(0, -s, 0), col);
            FDebugRenderer::DrawLine(p + glm::vec3(0, -s, 0), p + glm::vec3(0, 0, -s), col);
            FDebugRenderer::DrawLine(p + glm::vec3(0, 0, -s), p + glm::vec3(0, s, 0), col);

            glm::mat4 rot = glm::toMat4(glm::quat(glm::radians(tc.Rotation)));
            glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            FDebugRenderer::DrawArrow(p, p + forward * 1.25f, glm::vec4(1.0f, 0.75f, 0.15f, 1.0f), 0.2f);
        }

        // World origin XYZ axes (always useful when gizmos are on)
        {
            const float axisLen = 2.0f;
            FDebugRenderer::DrawArrow(glm::vec3(0.0f), glm::vec3(axisLen, 0.0f, 0.0f),
                                      glm::vec4(0.92f, 0.2f, 0.22f, 1.0f), 0.25f);
            FDebugRenderer::DrawArrow(glm::vec3(0.0f), glm::vec3(0.0f, axisLen, 0.0f),
                                      glm::vec4(0.2f, 0.85f, 0.3f, 1.0f), 0.25f);
            FDebugRenderer::DrawArrow(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, axisLen),
                                      glm::vec4(0.25f, 0.45f, 0.95f, 1.0f), 0.25f);
        }

        // Overlay without depth so volumes stay readable (matches game light-gizmo pass).
        FDebugRenderer::EndScene(false);
        fbo->Unbind();
    }

    void FViewportPanel::DrawViewportAxisIndicator(const ImVec2& InViewportMin, const ImVec2& InViewportSize) {
        if (InViewportSize.x < 80.0f || InViewportSize.y < 80.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float margin = 18.0f;
        const float axisLen = 42.0f;
        const ImVec2 origin(InViewportMin.x + margin + axisLen, InViewportMin.y + InViewportSize.y - margin - 8.0f);

        const glm::vec3 camRight = EditorCamera.GetRightDirection();
        const glm::vec3 camUp = EditorCamera.GetUpDirection();

        auto AxisTip = [&](const glm::vec3& worldAxis) -> ImVec2 {
            const float sx = glm::dot(worldAxis, camRight) * axisLen;
            const float sy = -glm::dot(worldAxis, camUp) * axisLen;
            return ImVec2(origin.x + sx, origin.y + sy);
        };

        auto DrawAxis = [&](const glm::vec3& worldAxis, ImU32 color, const char* label) {
            const ImVec2 tip = AxisTip(worldAxis);
            drawList->AddLine(origin, tip, color, 2.4f);
            drawList->AddCircleFilled(tip, 3.0f, color, 8);
            drawList->AddText(ImVec2(tip.x + 5.0f, tip.y - 7.0f), color, label);
        };

        drawList->AddCircleFilled(origin, 3.5f, IM_COL32(230, 230, 235, 220), 10);
        DrawAxis(glm::vec3(1.0f, 0.0f, 0.0f), IM_COL32(235, 55, 60, 255), "X");
        DrawAxis(glm::vec3(0.0f, 1.0f, 0.0f), IM_COL32(55, 210, 75, 255), "Y");
        DrawAxis(glm::vec3(0.0f, 0.0f, 1.0f), IM_COL32(65, 130, 245, 255), "Z");
    }

    void FViewportPanel::DrawEditorGizmoIcons(UWorld& InWorld, const ImVec2& InViewportMin,
                                              const ImVec2& InViewportSize) {
        if (!bShowEditorGizmos || InViewportSize.x <= 1.0f || InViewportSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

        auto Project = [&](const glm::vec3& world, ImVec2& out) -> bool {
            glm::vec4 clip = vp * glm::vec4(world, 1.0f);
            if (clip.w <= 0.001f)
                return false;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f)
                return false;
            out = ImVec2(InViewportMin.x + (ndc.x * 0.5f + 0.5f) * InViewportSize.x,
                         InViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InViewportSize.y);
            return true;
        };

        auto DrawBillboard = [&](const glm::vec3& world, ELucideIcon icon, ImU32 color, const char* label) {
            ImVec2 screen;
            if (!Project(world, screen))
                return;
            const float half = 10.0f;
            ImVec2 mn(screen.x - half, screen.y - half);
            ImVec2 mx(screen.x + half, screen.y + half);
            drawList->AddCircleFilled(screen, half + 2.0f, IM_COL32(20, 22, 28, 180), 16);
            FLucideIcons::DrawIcon(drawList, mn, mx, icon, color, 1.6f);
            if (label) {
                drawList->AddText(ImVec2(screen.x + half + 4.0f, screen.y - 7.0f), IM_COL32(230, 230, 235, 220),
                                  label);
            }
        };

        for (const auto& actorRef : InWorld.GetAllActors()) {
            AActor* actor = actorRef.get();
            if (!actor || actor->IsPendingKill() || !actor->HasComponent<FTransformComponent>())
                continue;
            const glm::vec3 pos = actor->GetComponent<FTransformComponent>().Translation;

            if (actor->HasComponent<FDirectionalLightComponent>() &&
                actor->GetComponent<FDirectionalLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Sun, IM_COL32(255, 220, 80, 255), "Dir");
            } else if (actor->HasComponent<FPointLightComponent>() &&
                       actor->GetComponent<FPointLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Lightbulb, IM_COL32(255, 180, 60, 255), "Point");
            } else if (actor->HasComponent<FSpotLightComponent>() &&
                       actor->GetComponent<FSpotLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Crosshair, IM_COL32(255, 140, 50, 255), "Spot");
            } else if (dynamic_cast<APlayerStart*>(actor) ||
                       actor->GetClass().find("PlayerStart") != std::string::npos ||
                       actor->GetName().find("PlayerStart") != std::string::npos) {
                DrawBillboard(pos, ELucideIcon::Waypoints, IM_COL32(80, 200, 255, 255), "Start");
            } else if (actor->HasComponent<FCameraComponent>()) {
                DrawBillboard(pos, ELucideIcon::Clapperboard, IM_COL32(200, 120, 255, 255), "Cam");
            }
        }
    }

    void FViewportPanel::DrawViewportToolbar() {
        float btnWidth = 26.0f;
        float btnHeight = 24.0f;
        ImVec2 toolBtnSize(btnWidth, btnHeight);

        // Tool Selection: icon-only (Q/W/E/R hotkeys still work via gizmo)
        auto DrawToolBtn = [&](EGizmoOperation Op, ELucideIcon Icon, const char* id, const char* tooltip) {
            const bool bActive = (Gizmo.GetOperation() == Op);
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            if (bActive) {
                ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + toolBtnSize.x, cursor.y + toolBtnSize.y),
                                                         IM_COL32(56, 115, 200, 255), 3.0f);
            }
            const ImU32 iconColor = bActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 225, 235, 255);
            const ImU32 hoverBg = bActive ? IM_COL32(70, 130, 220, 255) : IM_COL32(70, 74, 82, 255);
            if (FLucideIcons::IconButton(Icon, id, toolBtnSize, iconColor, hoverBg)) {
                Gizmo.SetOperation(Op);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::SameLine();
        };

        DrawToolBtn(EGizmoOperation::Select, ELucideIcon::MousePointer, "VpToolSelect", "Select (Q)");
        DrawToolBtn(EGizmoOperation::Translate, ELucideIcon::Move, "VpToolTranslate", "Translate (W)");
        DrawToolBtn(EGizmoOperation::Rotate, ELucideIcon::RefreshCw, "VpToolRotate", "Rotate (E)");
        DrawToolBtn(EGizmoOperation::Scale, ELucideIcon::Scaling, "VpToolScale", "Scale (R)");

        ImGui::Spacing();
        ImGui::SameLine();

        // Coordinate Space Toggle: World / Local
        bool bWorld = (Gizmo.GetMode() == EGizmoMode::World);
        if (FEditorWidgets::DrawToggleButton(bWorld ? ELucideIcon::Globe : ELucideIcon::Move, "##CoordSpace", true,
                                             bWorld ? "World" : "Local", ImVec2(0, btnHeight))) {
            Gizmo.SetMode(bWorld ? EGizmoMode::Local : EGizmoMode::World);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Coordinate Mode: %s", bWorld ? "World Space" : "Local Space");
        }

        ImGui::SameLine();

        // Snapping Toggle
        bool bSnap = Gizmo.IsSnappingEnabled();
        {
            FControlStyle snapStyle;
            if (bSnap) {
                snapStyle.bOverrideAccent = true;
                snapStyle.Accent = FEditorTheme::GetTokens().Success;
            }
            if (FEditorWidgets::DrawToggleButton(ELucideIcon::Magnet, "##SnapToggle", bSnap, "Snap",
                                                 ImVec2(0, btnHeight), &snapStyle)) {
                Gizmo.SetSnappingEnabled(!bSnap);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grid Snapping: %s", bSnap ? "Enabled" : "Disabled");
        }

        ImGui::SameLine();

        // Snapping Value Dropdown
        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::ChevronDown, "##SnapMenu", true, btnHeight)) {
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
        int vmIdx = static_cast<int>(ViewMode);
        {
            FControlStyle style;
            style.Width = 110.0f;
            if (FEditorWidgets::DrawSelect("##ViewModeCombo", &vmIdx, viewModeNames, 4, &style)) {
                ViewMode = static_cast<EViewportViewMode>(vmIdx);
            }
        }

        ImGui::SameLine();

        // Shading Mode: Lit / Unlit / Wireframe
        const char* shadingNames[] = {"Lit", "Unlit", "Wireframe"};
        int smIdx = static_cast<int>(ShadingMode);
        {
            FControlStyle style;
            style.Width = 100.0f;
            if (FEditorWidgets::DrawSelect("##ShadingCombo", &smIdx, shadingNames, 3, &style)) {
                ShadingMode = static_cast<EViewportShadingMode>(smIdx);
            }
        }

        ImGui::SameLine();

        // Stats Toggle
        if (FEditorWidgets::DrawToggleButton(ELucideIcon::Activity, "##StatsToggle", bShowStatistics,
                                             bShowStatistics ? "Hide Stats" : "Show Stats")) {
            bShowStatistics = !bShowStatistics;
        }

        ImGui::SameLine();
        if (FEditorWidgets::DrawToggleButton(bShowEditorGizmos ? ELucideIcon::Eye : ELucideIcon::EyeOff,
                                             "##GizmosToggle", bShowEditorGizmos,
                                             bShowEditorGizmos ? "Gizmos: On" : "Gizmos: Off")) {
            bShowEditorGizmos = !bShowEditorGizmos;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show Flags: editor gizmos (lights, PlayerStart, cameras)");
        }
    }

    void FViewportPanel::Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor,
                              bool* bInOutOpen) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::Viewport, bInOutOpen, ELucideIcon::Eye,
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        try {
            EnsureCamera();
            ProcessCameraInput();

            // Toolbar at top of viewport (above the 3D image — not overlapping pick coords)
            ImGui::SetCursorPos(ImVec2(8.0f, 28.0f));
            DrawViewportToolbar();

            const float imageTopY = ImGui::GetCursorPosY() + 6.0f;
            ImGui::SetCursorPos(ImVec2(0.0f, imageTopY));
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 vpSize(std::max(avail.x, 1.0f), std::max(avail.y, 1.0f));

            uint32_t w = std::max(1u, static_cast<uint32_t>(vpSize.x));
            uint32_t h = std::max(1u, static_cast<uint32_t>(vpSize.y));

            ImVec2 vpMin = ImGui::GetCursorScreenPos();
            bool bHasViewportImage = false;

            if (InWorld) {
                RenderWorld(*InWorld, w, h);

                if (WorldRenderer && WorldRenderer->GetHDRSceneFramebuffer()) {
                    uint32_t texId = WorldRenderer->GetHDRSceneFramebuffer()->GetColorAttachmentRendererID(0);
                    if (texId != 0) {
                        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(texId)), vpSize, ImVec2(0, 1),
                                     ImVec2(1, 0));
                        vpMin = ImGui::GetItemRectMin();
                        vpSize = ImGui::GetItemRectSize();
                        bHasViewportImage = true;
                    }
                }
            }

            // Active Primary Selected Actor
            AActor* activeActor = Context ? Context->GetSelection().GetPrimarySelectedActor() : InSelectedActor;

            std::vector<AActor*> gizmoActors;
            if (Context) {
                for (AActor* selected : Context->GetSelection().GetSelectedActors()) {
                    if (selected && !selected->IsPendingKill())
                        gizmoActors.push_back(selected);
                }
            } else if (activeActor && !activeActor->IsPendingKill()) {
                gizmoActors.push_back(activeActor);
            }

            // Draw 3D Interactive Transform Gizmo
            if (!gizmoActors.empty() && bHasViewportImage) {
                FEditorHistory* history = Context ? &Context->GetHistory() : nullptr;
                Gizmo.Draw(gizmoActors, EditorCamera, vpMin.x, vpMin.y, vpSize.x, vpSize.y, history);
            } else {
                // Critical: if Draw is skipped, hover/drag flags would stick and block all picking.
                Gizmo.CancelInteraction();
            }

            // Draw Selection Wireframe for all selected actors
            if (bHasViewportImage && Context) {
                for (AActor* selected : Context->GetSelection().GetSelectedActors()) {
                    if (selected && !selected->IsPendingKill())
                        DrawSelectionOutline(selected, vpMin, vpSize);
                }
            } else if (activeActor && !activeActor->IsPendingKill() && bHasViewportImage) {
                DrawSelectionOutline(activeActor, vpMin, vpSize);
            }

            // Unreal-like sprite icons for lights / PlayerStart / cameras
            if (InWorld && bHasViewportImage) {
                DrawEditorGizmoIcons(*InWorld, vpMin, vpSize);
            }

            // Always-on RGB axis indicator (bottom-left), Unreal viewport style
            if (bHasViewportImage) {
                DrawViewportAxisIndicator(vpMin, vpSize);
            }

            // Actor Selection and Raycast Picking
            // NOTE: ImGuiHoveredFlags_AllowWhenBlockedByActiveItem is required because
            // ImGui::Image() captures hover by default, which would prevent picking.
            const bool bWindowHoveredForPick = ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                ImGuiHoveredFlags_AllowWhenBlockedByPopup);

            bool bPickHandled = false;
            if (InWorld && bHasViewportImage && ImGui::IsMouseClicked(0) && bWindowHoveredForPick &&
                !Gizmo.IsDragging() && !Gizmo.IsHovered()) {
                ImVec2 mousePos = ImGui::GetMousePos();
                const bool bOverImage =
                    mousePos.x >= vpMin.x && mousePos.x <= vpMin.x + vpSize.x && mousePos.y >= vpMin.y &&
                    mousePos.y <= vpMin.y + vpSize.y;

                // Ctrl/Shift + click may start marquee; only consume as a point-pick without those mods,
                // or when modifiers are held but we still want toggle/add under the cursor (Unreal).
                if (bOverImage) {
                    glm::vec2 clickPos(mousePos.x, mousePos.y);
                    AActor* hitActor = PickActorAtScreenPos(*InWorld, clickPos, vpMin, vpSize);

                    bool bCtrl = ImGui::GetIO().KeyCtrl;
                    bool bShift = ImGui::GetIO().KeyShift;

                    if (Context) {
                        Context->ModifyActorSelectionWithUndo([&](FEditorSelection& selection) {
                            if (hitActor) {
                                if (bCtrl) {
                                    selection.ToggleActorSelection(hitActor);
                                } else if (bShift) {
                                    selection.SelectActor(hitActor, true);
                                } else {
                                    selection.SelectActor(hitActor, false);
                                }
                            } else if (!bCtrl && !bShift) {
                                selection.ClearActorSelection();
                            }
                        });
                    }

                    // Fire callback AFTER selection is updated in Context.
                    // Scrolls Outliner only — never moves the camera (Unreal: F / double-click).
                    if (OnActorSelected) {
                        OnActorSelected(hitActor);
                    }

                    // Let marquee start on modifier+empty drag; point picks always consume the click.
                    bPickHandled = hitActor != nullptr || (!bCtrl && !bShift);
                }
            }

            // Marquee Selection Box (only when picking did not consume the click)
            if (InWorld && bHasViewportImage && !bPickHandled) {
                ProcessMarqueeSelection(*InWorld, vpMin, vpSize);
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
                        if (pathStr.ends_with(".obj") || pathStr.ends_with(".fbx") || pathStr.ends_with(".gltf") ||
                            pathStr.ends_with(".lmesh")) {
                            glm::vec3 spawnPos = GetWorldRayIntersection(
                                glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y), vpMin, vpSize);
                            AActor* spawned = InWorld->SpawnActor<AActor>("DroppedMesh");
                            if (spawned) {
                                auto& tc = spawned->AddComponent<FTransformComponent>();
                                tc.Translation = spawnPos;
                                auto& sm = spawned->AddComponent<FStaticMeshComponent>();
                                sm.AssetPath = pathStr;
                                sm.StaticMesh = UAssetManager::GetStaticMesh(pathStr);
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

        const bool bHoveredForMarquee = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
            ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        if (ImGui::IsMouseClicked(0) && bHoveredForMarquee && !Gizmo.IsDragging()) {
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
                    Context->ModifyActorSelectionWithUndo([&](FEditorSelection& selection) {
                        if (io.KeyShift) {
                            for (AActor* a : enclosedActors) {
                                selection.SelectActor(a, true);
                            }
                        } else {
                            selection.SetSelectedActors(enclosedActors);
                        }
                    });
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

        auto RayIntersectsAABB = [](const glm::vec3& rOrigin, const glm::vec3& rDir, const glm::vec3& boxMin,
                                    const glm::vec3& boxMax, float& outT) -> bool {
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
                    if (t1 > t2)
                        std::swap(t1, t2);
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
            if (!actor || actor->IsPendingKill() || !actor->template HasComponent<FTransformComponent>())
                continue;

            const auto& tc = actor->template GetComponent<FTransformComponent>();
            glm::mat4 worldTransform = tc.GetTransform();
            glm::mat4 invWorld = glm::inverse(worldTransform);

            glm::vec3 localRayOrigin = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
            glm::vec3 localRayDir = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));
            float localDirLen = glm::length(localRayDir);
            if (localDirLen < 1e-6f)
                continue;
            localRayDir /= localDirLen;

            glm::vec3 boxMin, boxMax;
            GetActorEditorLocalBounds(*actor, boxMin, boxMax);

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

        // Secondary fallback: screen-distance for volume-less helpers (lights, cameras, empties).
        // Only consider actors that do NOT already have a mesh/collision volume — avoids stealing
        // hits when a large mesh AABB was slightly missed.
        if (!closestActor) {
            float minScreenDist = 24.0f;
            glm::mat4 viewProj = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

            for (const auto& actor : InWorld.GetAllActors()) {
                if (!actor || actor->IsPendingKill() || !actor->template HasComponent<FTransformComponent>())
                    continue;

                const bool bHasVolume = actor->template HasComponent<FStaticMeshComponent>() ||
                                        actor->template HasComponent<FMeshComponent>() ||
                                        actor->template HasComponent<FSkinnedMeshRenderState>() ||
                                        actor->template HasComponent<FBoxCollisionComponent>();
                if (bHasVolume)
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
        glm::mat4 model = tc.GetTransform();
        glm::mat4 vp = EditorCamera.GetProjectionMatrix() * EditorCamera.GetViewMatrix();

        glm::vec3 localMin, localMax;
        GetActorEditorLocalBounds(*InSelectedActor, localMin, localMax);

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

        // Oriented bounding wireframe from mesh/local AABB (matches pick volume)
        const glm::vec3 localCorners[8] = {
            {localMin.x, localMin.y, localMin.z}, {localMax.x, localMin.y, localMin.z},
            {localMax.x, localMax.y, localMin.z}, {localMin.x, localMax.y, localMin.z},
            {localMin.x, localMin.y, localMax.z}, {localMax.x, localMin.y, localMax.z},
            {localMax.x, localMax.y, localMax.z}, {localMin.x, localMax.y, localMax.z},
        };

        ImVec2 p[8];
        bool v[8];
        for (int i = 0; i < 8; ++i) {
            glm::vec3 world = glm::vec3(model * glm::vec4(localCorners[i], 1.0f));
            v[i] = Project3D(world, p[i]);
        }

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
