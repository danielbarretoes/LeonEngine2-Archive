#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "Editor/FViewportPanel.hpp"
#include "Core/FLog.hpp"
#include "Engine/Components.hpp"
#include <algorithm>
#include <cmath>

namespace Leon::Editor {

    void FViewportPanel::EnsureCamera() {
        if (!bCameraReady) {
            EditorCamera.SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));
            EditorCamera.SetRotation(-20.0f, -90.0f);
            bCameraReady = true;
        }
    }

    void FViewportPanel::FocusOnActor(AActor* InActor) {
        if (!InActor || !InActor->HasComponent<FTransformComponent>())
            return;

        const auto& trans = InActor->GetComponent<FTransformComponent>();
        glm::vec3 target = trans.Translation;
        glm::vec3 offset = glm::vec3(0.0f, 3.0f, 6.0f);
        EditorCamera.SetPosition(target + offset);
        EditorCamera.SetRotation(-20.0f, -90.0f);
    }

    void FViewportPanel::ProcessCameraInput() {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;

        // Adjust speed via mouse wheel when hovered
        if (bIsViewportHovered && io.MouseWheel != 0.0f) {
            CameraSpeed = std::clamp(CameraSpeed + io.MouseWheel * 1.5f, 1.0f, 100.0f);
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || !bIsViewportHovered) {
            LastMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
            return;
        }

        // Mouse look
        glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);
        glm::vec2 delta = mousePos - LastMousePos;
        LastMousePos = mousePos;

        constexpr float sensitivity = 0.15f;
        float pitch = EditorCamera.GetPitch() - delta.y * sensitivity;
        float yaw = EditorCamera.GetYaw() + delta.x * sensitivity;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        EditorCamera.SetRotation(pitch, yaw);

        // Keyboard movement
        float speed = CameraSpeed * dt;
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
            speed *= 2.5f;
        }

        glm::vec3 forward = EditorCamera.GetForwardDirection();
        glm::vec3 right = EditorCamera.GetRightDirection();
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 pos = EditorCamera.GetPosition();

        if (ImGui::IsKeyDown(ImGuiKey_W))
            pos += forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S))
            pos -= forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A))
            pos -= right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
            pos += right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E))
            pos += up * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            pos -= up * speed;

        EditorCamera.SetPosition(pos);
    }

    void FViewportPanel::RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight) {
        if (!WorldRenderer || WorldRendererWorld != &InWorld) {
            WorldRenderer = std::make_unique<FWorldRenderer>(&InWorld);
            WorldRendererWorld = &InWorld;
        }

        WorldRenderer->SetWireframeEnabled(ShadingMode == EViewportShadingMode::Wireframe);
        WorldRenderer->OnViewportResize(InWidth, InHeight);
        WorldRenderer->Render(EditorCamera);

        auto fbo = WorldRenderer->GetHDRSceneFramebuffer();
        if (fbo) {
            uint32_t texId = fbo->GetColorAttachmentRendererID(0);
            ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(texId)),
                         ImVec2(static_cast<float>(InWidth), static_cast<float>(InHeight)), ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    void FViewportPanel::DrawViewportOverlay(const std::string& InMapName, AActor* InSelectedActor,
                                             uint32_t InActorCount) {
        (void)InSelectedActor;
        ImVec2 winSize = ImGui::GetWindowSize();

        // Top Toolbar Strip inside Viewport
        ImGui::SetCursorPos(ImVec2(12.0f, 32.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 0.85f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));

        if (ImGui::BeginChild("ViewportControlStrip", ImVec2(winSize.x - 24.0f, 34.0f), false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding)) {
            // View Mode
            const char* viewModes[] = {"Perspective", "Top", "Front", "Side"};
            int currentView = static_cast<int>(ViewMode);
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::Combo("##ViewModeCombo", &currentView, viewModes, IM_ARRAYSIZE(viewModes))) {
                ViewMode = static_cast<EViewportViewMode>(currentView);
            }

            ImGui::SameLine();

            // Shading Mode
            const char* shadingModes[] = {"Lit", "Unlit", "Wireframe"};
            int currentShading = static_cast<int>(ShadingMode);
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##ShadingCombo", &currentShading, shadingModes, IM_ARRAYSIZE(shadingModes))) {
                ShadingMode = static_cast<EViewportShadingMode>(currentShading);
            }

            ImGui::SameLine();

            // Show Flags
            if (ImGui::Button("Show Options")) {
                ImGui::OpenPopup("ShowOptionsPopup");
            }

            if (ImGui::BeginPopup("ShowOptionsPopup")) {
                ImGui::Checkbox("Grid", &bShowGrid);
                ImGui::Checkbox("Selection Bounds", &bShowSelectionBounds);
                ImGui::Checkbox("Stats Overlay", &bShowStatsOverlay);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Camera Speed Slider
            ImGui::Text("Cam Speed:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::SliderFloat("##CamSpeed", &CameraSpeed, 1.0f, 50.0f, "%.1f");

            // Gizmo Operation shortcuts
            ImGui::SameLine(winSize.x - 210.0f);
            bool bTrans = (Gizmo.GetOperation() == EGizmoOperation::Translate);
            bool bRot = (Gizmo.GetOperation() == EGizmoOperation::Rotate);
            bool bScale = (Gizmo.GetOperation() == EGizmoOperation::Scale);

            if (bTrans)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("W: Move"))
                Gizmo.SetOperation(EGizmoOperation::Translate);
            if (bTrans)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            if (bRot)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("E: Rotate"))
                Gizmo.SetOperation(EGizmoOperation::Rotate);
            if (bRot)
                ImGui::PopStyleColor();

            ImGui::SameLine();
            if (bScale)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("R: Scale"))
                Gizmo.SetOperation(EGizmoOperation::Scale);
            if (bScale)
                ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        // Bottom Stats Overlay
        if (bShowStatsOverlay) {
            ImGui::SetCursorPos(ImVec2(12.0f, winSize.y - 70.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 0.75f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

            if (ImGui::BeginChild("ViewportStatsOverlay", ImVec2(340.0f, 58.0f), false,
                                  ImGuiWindowFlags_AlwaysUseWindowPadding)) {
                ImGuiIO& io = ImGui::GetIO();
                glm::vec3 pos = EditorCamera.GetPosition();
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Level: %s", InMapName.c_str());
                ImGui::SameLine(180.0f);
                ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate,
                            1000.0f / (io.Framerate > 0.0f ? io.Framerate : 60.0f));

                ImGui::TextDisabled("Actors: %u  |  Cam: [%.1f, %.1f, %.1f]", InActorCount, pos.x, pos.y, pos.z);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
    }

    void FViewportPanel::Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        EnsureCamera();

        bIsViewportFocused = ImGui::IsWindowFocused();
        bIsViewportHovered = ImGui::IsWindowHovered();

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        uint32_t width = static_cast<uint32_t>(std::max(1.0f, viewportPanelSize.x));
        uint32_t height = static_cast<uint32_t>(std::max(1.0f, viewportPanelSize.y));

        if (width != LastWidth || height != LastHeight) {
            LastWidth = width;
            LastHeight = height;
            EditorCamera.SetViewportSize(width, height);
        }

        ProcessCameraInput();

        uint32_t actorCount = 0;
        if (InWorld) {
            actorCount = static_cast<uint32_t>(InWorld->GetAllActors().size());
            RenderWorld(*InWorld, width, height);

            // Gizmo manipulation
            if (InSelectedActor) {
                ImVec2 min = ImGui::GetItemRectMin();
                Gizmo.Draw(InSelectedActor, EditorCamera, min.x, min.y, static_cast<float>(width),
                           static_cast<float>(height));
            }
        } else {
            ImGui::TextDisabled("No active level to render.");
        }

        DrawViewportOverlay(InMapName, InSelectedActor, actorCount);

        ImGui::End();
        ImGui::PopStyleVar();
    }

} // namespace Leon::Editor
