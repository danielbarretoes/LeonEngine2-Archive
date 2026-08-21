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
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
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

        glm::vec3 ActorWorldOrigin(AActor& InActor) {
            return glm::vec3(InActor.GetActorWorldMatrix()[3]);
        }

        bool ProjectWorld(const glm::mat4& InViewProj, const glm::vec3& InWorld, const ImVec2& InMin,
                          const ImVec2& InSize, ImVec2& OutScreen) {
            glm::vec4 clip = InViewProj * glm::vec4(InWorld, 1.0f);
            if (clip.w <= 0.001f)
                return false;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            OutScreen = ImVec2(InMin.x + (ndc.x * 0.5f + 0.5f) * InSize.x,
                               InMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InSize.y);
            return true;
        }

        ImVec2 ClipToScreen(const glm::vec4& InClip, const ImVec2& InMin, const ImVec2& InSize) {
            const glm::vec3 ndc = glm::vec3(InClip) / InClip.w;
            return ImVec2(InMin.x + (ndc.x * 0.5f + 0.5f) * InSize.x,
                          InMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * InSize.y);
        }

        bool ClipLineToRect(ImVec2& InOutA, ImVec2& InOutB, const ImVec2& InMin, const ImVec2& InMax) {
            const float dx = InOutB.x - InOutA.x;
            const float dy = InOutB.y - InOutA.y;
            float t0 = 0.0f;
            float t1 = 1.0f;
            auto Clip = [&](float p, float q) -> bool {
                if (std::abs(p) < 1e-8f)
                    return q >= 0.0f;
                const float r = q / p;
                if (p < 0.0f) {
                    if (r > t1)
                        return false;
                    if (r > t0)
                        t0 = r;
                } else {
                    if (r < t0)
                        return false;
                    if (r < t1)
                        t1 = r;
                }
                return true;
            };
            if (!Clip(-dx, InOutA.x - InMin.x) || !Clip(dx, InMax.x - InOutA.x) || !Clip(-dy, InOutA.y - InMin.y) ||
                !Clip(dy, InMax.y - InOutA.y))
                return false;
            const ImVec2 origin = InOutA;
            InOutA = ImVec2(origin.x + t0 * dx, origin.y + t0 * dy);
            InOutB = ImVec2(origin.x + t1 * dx, origin.y + t1 * dy);
            return true;
        }

        /** Clip a world-space segment to the near plane, project, then clip to the viewport.
         *  Only the near plane is clipped in 3D — x/y frustum clip at w≈0 shoots a line
         *  across the screen through the camera. */
        bool ClipProjectLine(const glm::mat4& InViewProj, const glm::vec3& InA, const glm::vec3& InB,
                             const ImVec2& InMin, const ImVec2& InSize, ImVec2& OutA, ImVec2& OutB) {
            glm::vec4 a = InViewProj * glm::vec4(InA, 1.0f);
            glm::vec4 b = InViewProj * glm::vec4(InB, 1.0f);

            // OpenGL near plane: ndc.z >= -1 → z + w >= 0
            const float d0 = a.z + a.w;
            const float d1 = b.z + b.w;
            const bool in0 = d0 >= 0.0f;
            const bool in1 = d1 >= 0.0f;
            if (!in0 && !in1)
                return false;
            if (in0 != in1) {
                const float denom = d0 - d1;
                if (std::abs(denom) < 1e-12f)
                    return false;
                const glm::vec4 hit = a + (b - a) * (d0 / denom);
                if (!in0)
                    a = hit;
                else
                    b = hit;
            }

            if (a.w <= 0.05f || b.w <= 0.05f)
                return false;

            OutA = ClipToScreen(a, InMin, InSize);
            OutB = ClipToScreen(b, InMin, InSize);

            const ImVec2 rectMax(InMin.x + InSize.x, InMin.y + InSize.y);
            return ClipLineToRect(OutA, OutB, InMin, rectMax);
        }

        bool UnprojectScreenRay(const FPerspectiveCamera& InCamera, const glm::vec2& InScreen, const ImVec2& InMin,
                                const ImVec2& InSize, glm::vec3& OutOrigin, glm::vec3& OutDir) {
            if (InSize.x <= 1e-4f || InSize.y <= 1e-4f)
                return false;
            const float ndcX = ((InScreen.x - InMin.x) / InSize.x) * 2.0f - 1.0f;
            const float ndcY = 1.0f - ((InScreen.y - InMin.y) / InSize.y) * 2.0f;
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

        bool IsPlayerStartActor(AActor& InActor) {
            return dynamic_cast<APlayerStart*>(&InActor) != nullptr ||
                   InActor.GetClass().find("PlayerStart") != std::string::npos ||
                   InActor.GetName().find("PlayerStart") != std::string::npos;
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
            PivotPoint = glm::vec3(0.0f, 0.0f, 0.0f);
            bCameraInitialized = true;
        }
    }

    void FViewportPanel::FocusOnActor(AActor* InActor) {
        if (!InActor || !InActor->HasComponent<FTransformComponent>()) {
            return;
        }

        glm::vec3 localMin, localMax;
        GetActorEditorLocalBounds(*InActor, localMin, localMax);

        glm::vec3 worldMin, worldMax;
        TransformAABBCorners(localMin, localMax, InActor->GetActorWorldMatrix(), worldMin, worldMax);
        PivotPoint = (worldMin + worldMax) * 0.5f;
        const float radius = glm::length(worldMax - worldMin) * 0.5f;
        const float dist = std::max(radius * 2.5f, 2.0f);

        if (ViewMode != EViewportViewMode::Perspective) {
            OrthoHeight = std::max(radius * 3.0f, 4.0f);
            EditorCamera.SetPosition(PivotPoint - EditorCamera.GetForwardDirection() * dist);
            EditorCamera.SetOrthoHeight(OrthoHeight);
            return;
        }

        glm::vec3 forward = EditorCamera.GetForwardDirection();
        EditorCamera.SetPosition(PivotPoint - forward * dist);
    }

    void FViewportPanel::OrbitAroundPivot(float InYawDelta, float InPitchDelta) {
        float dist = glm::length(EditorCamera.GetPosition() - PivotPoint);
        dist = std::max(dist, 0.25f);
        float yaw = EditorCamera.GetYaw() + InYawDelta;
        float pitch = std::clamp(EditorCamera.GetPitch() + InPitchDelta, -89.0f, 89.0f);
        EditorCamera.SetRotation(pitch, yaw);
        EditorCamera.SetPosition(PivotPoint - EditorCamera.GetForwardDirection() * dist);
    }

    void FViewportPanel::PanCamera(float InDeltaX, float InDeltaY) {
        const float dist = std::max(glm::length(EditorCamera.GetPosition() - PivotPoint), 1.0f);
        const float scale = EditorCamera.IsOrthographic() ? (OrthoHeight * 0.0025f) : (dist * 0.0025f);
        const glm::vec3 delta = (-EditorCamera.GetRightDirection() * InDeltaX + EditorCamera.GetUpDirection() * InDeltaY) *
                                scale;
        EditorCamera.SetPosition(EditorCamera.GetPosition() + delta);
        PivotPoint += delta;
    }

    void FViewportPanel::DollyCamera(float InAmount) {
        if (EditorCamera.IsOrthographic() || ViewMode != EViewportViewMode::Perspective) {
            OrthoHeight = std::clamp(OrthoHeight * (1.0f - InAmount * 0.12f), 0.5f, 400.0f);
            EditorCamera.SetOrthoHeight(OrthoHeight);
            return;
        }
        glm::vec3 pos = EditorCamera.GetPosition();
        glm::vec3 forward = EditorCamera.GetForwardDirection();
        pos += forward * InAmount * CameraSpeed * 0.45f;
        EditorCamera.SetPosition(pos);
        // Keep pivot in front of the camera so orbit stays stable.
        const float along = glm::dot(PivotPoint - pos, forward);
        if (along < 0.5f)
            PivotPoint = pos + forward * std::max(along, 2.0f);
    }

    void FViewportPanel::ApplyViewMode() {
        if (ViewMode == AppliedViewMode)
            return;

        if (AppliedViewMode == EViewportViewMode::Perspective && ViewMode != EViewportViewMode::Perspective) {
            SavedPerspPosition = EditorCamera.GetPosition();
            SavedPerspPitch = EditorCamera.GetPitch();
            SavedPerspYaw = EditorCamera.GetYaw();
        }

        AppliedViewMode = ViewMode;
        const float dist = std::max(glm::length(EditorCamera.GetPosition() - PivotPoint), 8.0f);

        if (ViewMode == EViewportViewMode::Perspective) {
            EditorCamera.SetOrthographic(false);
            EditorCamera.SetPosition(SavedPerspPosition);
            EditorCamera.SetRotation(SavedPerspPitch, SavedPerspYaw);
            return;
        }

        if (ViewMode == EViewportViewMode::Top)
            EditorCamera.SetRotation(-89.0f, -90.0f);
        else if (ViewMode == EViewportViewMode::Front)
            EditorCamera.SetRotation(0.0f, -90.0f);
        else
            EditorCamera.SetRotation(0.0f, 180.0f);

        EditorCamera.SetPosition(PivotPoint - EditorCamera.GetForwardDirection() * dist);
        EditorCamera.SetOrthographic(true, OrthoHeight);
    }

    void FViewportPanel::ProcessCameraInput() {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        if (dt <= 0.0f || dt > 0.1f)
            dt = 0.016f;

        const bool bWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool bAlt = io.KeyAlt;

        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !io.WantTextInput) {
            AActor* primaryActor = Context ? Context->GetSelection().GetPrimarySelectedActor() : nullptr;
            if (primaryActor)
                FocusOnActor(primaryActor);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && bWindowHovered && !bAlt)
            bRmbNavigating = true;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            bRmbNavigating = false;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && bWindowHovered)
            bMmbPanning = true;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
            bMmbPanning = false;

        if (bAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && bWindowHovered)
            bAltOrbiting = true;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            bAltOrbiting = false;

        if (bWindowHovered && io.MouseWheel != 0.0f) {
            if (bRmbNavigating) {
                CameraSpeed = std::clamp(CameraSpeed + io.MouseWheel * 1.5f, 1.0f, 50.0f);
            } else {
                DollyCamera(io.MouseWheel);
            }
        }

        if (bMmbPanning || (bAlt && ImGui::IsMouseDown(ImGuiMouseButton_Middle))) {
            PanCamera(io.MouseDelta.x, io.MouseDelta.y);
        } else if (bAltOrbiting || (bAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            OrbitAroundPivot(io.MouseDelta.x * 0.25f, -io.MouseDelta.y * 0.25f);
        } else if (bAlt && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            DollyCamera(io.MouseDelta.y * 0.05f);
        } else if (bRmbNavigating && ViewMode == EViewportViewMode::Perspective) {
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
                const glm::vec3 step = glm::normalize(moveDir) * speed * dt;
                EditorCamera.SetPosition(EditorCamera.GetPosition() + step);
                PivotPoint += step;
            }

            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                const float sens = 0.15f;
                float yaw = EditorCamera.GetYaw() + io.MouseDelta.x * sens;
                float pitch = std::clamp(EditorCamera.GetPitch() - io.MouseDelta.y * sens, -89.0f, 89.0f);
                EditorCamera.SetRotation(pitch, yaw);
            }
        } else if (bRmbNavigating && ViewMode != EViewportViewMode::Perspective) {
            PanCamera(io.MouseDelta.x, io.MouseDelta.y);
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

        const float aspect = static_cast<float>(InWidth) / static_cast<float>(InHeight);
        FPerspectiveCamera* Camera = &EditorCamera;
        FPerspectiveCamera PlayCamera(45.0f, aspect, 0.1f, 1000.0f);

        if (bPlayingInEditor) {
            if (APlayerController* Pc = InWorld.GetFirstPlayerController()) {
                if (Pc->GetPlayerCameraManager())
                    Pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                Pc->GetPlayerViewPoint(PlayCamera);
                Camera = &PlayCamera;
            } else {
                EditorCamera.SetProjection(45.0f, aspect, 0.1f, 1000.0f);
            }
        } else if (ViewMode == EViewportViewMode::Perspective) {
            EditorCamera.SetProjection(45.0f, aspect, 0.1f, 1000.0f);
        } else {
            EditorCamera.SetViewportSize(InWidth, InHeight);
            EditorCamera.SetOrthographic(true, OrthoHeight);
        }

        WorldRenderer->SetWireframeEnabled(ShadingMode == EViewportShadingMode::Wireframe);
        WorldRenderer->SetDebugMode(ShadingMode == EViewportShadingMode::Unlit ? 14 : 0);
        WorldRenderer->Render(*Camera);

        if (bShowEditorGizmos && !bPlayingInEditor) {
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
            glm::vec3 pos = transform.Translation;
            if (AActor* actor = InWorld.FindActorByEntity(entity))
                pos = ActorWorldOrigin(*actor);
            FDebugRenderer::DrawDirectionalLightGizmo(light, pos, 2.5f);
        }

        auto pointView = reg.view<FPointLightComponent, FTransformComponent>();
        for (auto entity : pointView) {
            auto [pointComp, transform] = pointView.get<FPointLightComponent, FTransformComponent>(entity);
            if (!pointComp.bEnabled)
                continue;
            FPointLight light = pointComp.Light;
            light.Position = transform.Translation;
            if (AActor* actor = InWorld.FindActorByEntity(entity))
                light.Position = ActorWorldOrigin(*actor);
            FDebugRenderer::DrawPointLightGizmo(light);
        }

        auto spotView = reg.view<FSpotLightComponent, FTransformComponent>();
        for (auto entity : spotView) {
            auto [spotComp, transform] = spotView.get<FSpotLightComponent, FTransformComponent>(entity);
            if (!spotComp.bEnabled)
                continue;
            FSpotLight light = spotComp.Light;
            glm::vec3 pos = transform.Translation;
            glm::vec3 euler = transform.Rotation;
            if (AActor* actor = InWorld.FindActorByEntity(entity)) {
                pos = ActorWorldOrigin(*actor);
                euler = actor->GetActorRotation();
            }
            SyncSpotLightFromTransform(light, pos, euler);
            // Keep component in sync so lighting / save match the gizmo.
            spotComp.Light.Position = light.Position;
            spotComp.Light.Direction = light.Direction;
            FDebugRenderer::DrawSpotLightGizmo(light);
        }

            // PlayerStart: diamond + forward arrow (Unreal-like spawn marker)
        for (const auto& actorRef : InWorld.GetAllActors()) {
            AActor* actor = actorRef.get();
            if (!actor || actor->IsPendingKill() || !actor->HasComponent<FTransformComponent>())
                continue;
            if (!IsPlayerStartActor(*actor))
                continue;

            const glm::vec3 p = ActorWorldOrigin(*actor);
            const auto& tc = actor->GetComponent<FTransformComponent>();
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
        glm::mat4 vp = EditorCamera.GetViewProjectionMatrix();

        auto Project = [&](const glm::vec3& world, ImVec2& out) -> bool {
            return ProjectWorld(vp, world, InViewportMin, InViewportSize, out);
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
            const glm::vec3 pos = ActorWorldOrigin(*actor);

            if (actor->HasComponent<FDirectionalLightComponent>() &&
                actor->GetComponent<FDirectionalLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Sun, IM_COL32(255, 220, 80, 255), "Dir");
            } else if (actor->HasComponent<FPointLightComponent>() &&
                       actor->GetComponent<FPointLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Lightbulb, IM_COL32(255, 180, 60, 255), "Point");
            } else if (actor->HasComponent<FSpotLightComponent>() &&
                       actor->GetComponent<FSpotLightComponent>().bEnabled) {
                DrawBillboard(pos, ELucideIcon::Crosshair, IM_COL32(255, 140, 50, 255), "Spot");
            } else if (IsPlayerStartActor(*actor)) {
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
            if (!bPlayingInEditor) {
                ApplyViewMode();
                ProcessCameraInput();
            }

            if (!bPlayingInEditor && !ImGui::GetIO().WantTextInput && !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
                (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) || ImGui::IsWindowFocused())) {
                Gizmo.ProcessHotkeys();
            }

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
            if (!bPlayingInEditor && !gizmoActors.empty() && bHasViewportImage) {
                FEditorHistory* history = Context ? &Context->GetHistory() : nullptr;
                Gizmo.Draw(gizmoActors, EditorCamera, vpMin.x, vpMin.y, vpSize.x, vpSize.y, history);
            } else {
                // Critical: if Draw is skipped, hover/drag flags would stick and block all picking.
                Gizmo.CancelInteraction();
            }

            // Draw Selection Wireframe for all selected actors
            if (!bPlayingInEditor && bHasViewportImage && Context) {
                for (AActor* selected : Context->GetSelection().GetSelectedActors()) {
                    if (selected && !selected->IsPendingKill())
                        DrawSelectionOutline(selected, vpMin, vpSize);
                }
            } else if (!bPlayingInEditor && activeActor && !activeActor->IsPendingKill() && bHasViewportImage) {
                DrawSelectionOutline(activeActor, vpMin, vpSize);
            }

            // Unreal-like sprite icons for lights / PlayerStart / cameras
            if (!bPlayingInEditor && InWorld && bHasViewportImage) {
                DrawEditorGizmoIcons(*InWorld, vpMin, vpSize);
            }

            // Always-on RGB axis indicator (bottom-left), Unreal viewport style
            if (bHasViewportImage) {
                DrawViewportAxisIndicator(vpMin, vpSize);
            }

            // Actor Selection and Raycast Picking
            const bool bWindowHoveredForPick = ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                ImGuiHoveredFlags_AllowWhenBlockedByPopup);
            const bool bCameraBusy = bRmbNavigating || bMmbPanning || bAltOrbiting || ImGui::GetIO().KeyAlt;

            bool bPickHandled = false;
            if (!bPlayingInEditor && InWorld && bHasViewportImage && ImGui::IsMouseClicked(0) && bWindowHoveredForPick &&
                !Gizmo.IsDragging() && !Gizmo.IsHovered() && !bCameraBusy) {
                ImVec2 mousePos = ImGui::GetMousePos();
                const bool bOverImage =
                    mousePos.x >= vpMin.x && mousePos.x <= vpMin.x + vpSize.x && mousePos.y >= vpMin.y &&
                    mousePos.y <= vpMin.y + vpSize.y;

                if (bOverImage) {
                    glm::vec2 clickPos(mousePos.x, mousePos.y);
                    AActor* hitActor = PickActorAtScreenPos(*InWorld, clickPos, vpMin, vpSize);

                    if (ImGui::IsMouseDoubleClicked(0) && hitActor) {
                        FocusOnActor(hitActor);
                    }

                    bool bCtrl = ImGui::GetIO().KeyCtrl;
                    bool bShift = ImGui::GetIO().KeyShift;

                    if (hitActor) {
                        if (Context) {
                            Context->ModifyActorSelectionWithUndo([&](FEditorSelection& selection) {
                                if (bCtrl)
                                    selection.ToggleActorSelection(hitActor);
                                else if (bShift)
                                    selection.SelectActor(hitActor, true);
                                else
                                    selection.SelectActor(hitActor, false);
                            });
                        }
                        if (OnActorSelected)
                            OnActorSelected(hitActor);
                        bPickHandled = true;
                    } else {
                        // Empty click: start marquee (commit/clear on release).
                        bPickHandled = false;
                    }
                }
            }

            if (InWorld && bHasViewportImage && !bPickHandled && !bCameraBusy) {
                ProcessMarqueeSelection(*InWorld, vpMin, vpSize);
            }

            if (InWorld && bHasViewportImage) {
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
                    const bool bPlace = payload->IsDataType("PLACE_ACTOR_TYPE");
                    const bool bMesh = payload->IsDataType("CONTENT_BROWSER_ASSET");
                    if (bPlace || bMesh) {
                        glm::vec3 ghostPos = GetWorldRayIntersection(
                            glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y), vpMin, vpSize);
                        const char* label = "Place";
                        if (bPlace)
                            label = static_cast<const char*>(payload->Data);
                        else if (payload->Data)
                            label = static_cast<const char*>(payload->Data);
                        DrawPlacementGhost(ghostPos, vpMin, vpSize, label ? label : "Place");
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
        if (ImGui::IsMouseClicked(0) && bHoveredForMarquee && !Gizmo.IsDragging() && !Gizmo.IsHovered() &&
            !bRmbNavigating && !bMmbPanning && !bAltOrbiting) {
            const ImVec2 mp = io.MousePos;
            const bool bOverImage = mp.x >= InViewportMin.x && mp.x <= InViewportMin.x + InViewportSize.x &&
                                    mp.y >= InViewportMin.y && mp.y <= InViewportMin.y + InViewportSize.y;
            if (bOverImage) {
                bMarqueeSelecting = true;
                MarqueeStart = mp;
                MarqueeEnd = mp;
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

                const float dragPx = std::max(std::abs(MarqueeEnd.x - MarqueeStart.x),
                                              std::abs(MarqueeEnd.y - MarqueeStart.y));
                if (dragPx < 4.0f) {
                    if (!io.KeyCtrl && !io.KeyShift && Context) {
                        Context->ModifyActorSelectionWithUndo(
                            [&](FEditorSelection& selection) { selection.ClearActorSelection(); });
                        if (OnActorSelected)
                            OnActorSelected(nullptr);
                    }
                    return;
                }

                glm::mat4 vp = EditorCamera.GetViewProjectionMatrix();
                std::vector<AActor*> enclosedActors;

                for (const auto& actor : InWorld.GetAllActors()) {
                    if (!actor || actor->IsPendingKill() || !actor->template HasComponent<FTransformComponent>())
                        continue;

                    ImVec2 screenPos;
                    if (!ProjectWorld(vp, ActorWorldOrigin(*actor), InViewportMin, InViewportSize, screenPos))
                        continue;

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

        glm::vec3 rayOrigin, rayDir;
        if (!UnprojectScreenRay(EditorCamera, InScreenPos, InViewportMin, InViewportSize, rayOrigin, rayDir))
            return nullptr;

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

            glm::mat4 worldTransform = actor->GetActorWorldMatrix();
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
            glm::mat4 viewProj = EditorCamera.GetViewProjectionMatrix();

            for (const auto& actor : InWorld.GetAllActors()) {
                if (!actor || actor->IsPendingKill() || !actor->template HasComponent<FTransformComponent>())
                    continue;

                const bool bHasVolume = actor->template HasComponent<FStaticMeshComponent>() ||
                                        actor->template HasComponent<FMeshComponent>() ||
                                        actor->template HasComponent<FSkinnedMeshRenderState>() ||
                                        actor->template HasComponent<FBoxCollisionComponent>();
                if (bHasVolume)
                    continue;

                ImVec2 screen;
                if (!ProjectWorld(viewProj, ActorWorldOrigin(*actor), InViewportMin, InViewportSize, screen))
                    continue;
                float screenDist = glm::length(glm::vec2(screen.x, screen.y) - InScreenPos);

                if (screenDist < minScreenDist) {
                    minScreenDist = screenDist;
                    closestActor = actor.get();
                }
            }
        }

        return closestActor;
    }

    glm::vec3 FViewportPanel::GetWorldRayIntersection(const glm::vec2& InScreenPos, const ImVec2& InViewportMin,
                                                      const ImVec2& InViewportSize) {
        glm::vec3 rayOrigin, rayDir;
        if (!UnprojectScreenRay(EditorCamera, InScreenPos, InViewportMin, InViewportSize, rayOrigin, rayDir))
            return glm::vec3(0.0f);

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

        glm::mat4 model = InSelectedActor->GetActorWorldMatrix();
        glm::mat4 vp = EditorCamera.GetViewProjectionMatrix();

        glm::vec3 localMin, localMax;
        GetActorEditorLocalBounds(*InSelectedActor, localMin, localMax);

        // Sit just outside the mesh so coincident edges don't vanish into the surface.
        const glm::vec3 pad = glm::max((localMax - localMin) * 0.012f, glm::vec3(0.02f));
        localMin -= pad;
        localMax += pad;

        const glm::vec3 localCorners[8] = {
            {localMin.x, localMin.y, localMin.z}, {localMax.x, localMin.y, localMin.z},
            {localMax.x, localMax.y, localMin.z}, {localMin.x, localMax.y, localMin.z},
            {localMin.x, localMin.y, localMax.z}, {localMax.x, localMin.y, localMax.z},
            {localMax.x, localMax.y, localMax.z}, {localMin.x, localMax.y, localMax.z},
        };

        glm::vec3 worldCorners[8];
        for (int i = 0; i < 8; ++i)
            worldCorners[i] = glm::vec3(model * glm::vec4(localCorners[i], 1.0f));

        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(InViewportMin,
                               ImVec2(InViewportMin.x + InViewportSize.x, InViewportMin.y + InViewportSize.y), true);

        const ImU32 outlineBack = IM_COL32(20, 12, 0, 230);
        const ImU32 outlineCol = IM_COL32(255, 175, 20, 255);

        for (int i = 0; i < 12; ++i) {
            ImVec2 pa, pb;
            if (!ClipProjectLine(vp, worldCorners[edges[i][0]], worldCorners[edges[i][1]], InViewportMin,
                                 InViewportSize, pa, pb))
                continue;
            drawList->AddLine(pa, pb, outlineBack, 3.4f);
            drawList->AddLine(pa, pb, outlineCol, 1.8f);
        }

        drawList->PopClipRect();
    }

    void FViewportPanel::DrawPlacementGhost(const glm::vec3& InWorldPos, const ImVec2& InViewportMin,
                                            const ImVec2& InViewportSize, const char* InLabel) {
        glm::mat4 vp = EditorCamera.GetViewProjectionMatrix();

        auto Project3D = [&](const glm::vec3& p, ImVec2& out) -> bool {
            return ProjectWorld(vp, p, InViewportMin, InViewportSize, out);
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
        snprintf(statsText, sizeof(statsText),
                 "Map: %s  |  FPS: %.0f (%.1f ms)  |  Speed: %.0f  |  Actors: %u  |  Selected: %zu (%s)",
                 InMapName.empty() ? "Untitled" : InMapName.c_str(), FrameRate, FrameTimeMs, CameraSpeed, InActorCount,
                 selCount, selName);

        drawList->AddText(ImVec2(overlayPos.x + 8.0f, overlayPos.y + 4.0f), IM_COL32(200, 205, 215, 255), statsText);
    }

} // namespace Leon::Editor
