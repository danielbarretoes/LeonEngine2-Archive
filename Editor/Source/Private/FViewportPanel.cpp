#include "Editor/FViewportPanel.hpp"
#include "RHI/FFramebuffer.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Engine/Components.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstdint>

namespace Leon::Editor {

    void FViewportPanel::EnsureCamera() {
        if (bCameraReady) {
            return;
        }
        EditorCamera.SetPosition({0.0f, 4.0f, 10.0f});
        EditorCamera.SetRotation(-20.0f, -90.0f);
        bCameraReady = true;
    }

    void FViewportPanel::FocusOnActor(AActor* InActor) {
        if (!InActor || !InActor->HasComponent<FTransformComponent>())
            return;

        const auto& tc = InActor->GetComponent<FTransformComponent>();
        glm::vec3 forward = EditorCamera.GetForwardDirection();
        EditorCamera.SetPosition(tc.Translation - forward * 5.0f);
    }

    void FViewportPanel::ProcessCameraInput() {
        if (!bIsViewportHovered && !bIsViewportFocused)
            return;

        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;

        // Camera Speed adjustment with mouse wheel
        if (bIsViewportHovered && io.MouseWheel != 0.0f) {
            CameraSpeed = std::clamp(CameraSpeed + io.MouseWheel * 2.0f, 1.0f, 100.0f);
        }

        // Right Mouse Button: Free-fly WASD + Look
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            glm::vec2 mousePos{io.MousePos.x, io.MousePos.y};
            glm::vec2 delta = (mousePos - LastMousePos) * 0.15f;

            float pitch = std::clamp(EditorCamera.GetPitch() - delta.y, -89.0f, 89.0f);
            float yaw = EditorCamera.GetYaw() + delta.x;
            EditorCamera.SetRotation(pitch, yaw);

            float speed = CameraSpeed;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
                speed *= 2.5f;

            glm::vec3 pos = EditorCamera.GetPosition();
            glm::vec3 forward = EditorCamera.GetForwardDirection();
            glm::vec3 right = EditorCamera.GetRightDirection();
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

            if (ImGui::IsKeyDown(ImGuiKey_W))
                pos += forward * speed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_S))
                pos -= forward * speed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_D))
                pos += right * speed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_A))
                pos -= right * speed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_E))
                pos += up * speed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_Q))
                pos -= up * speed * dt;

            EditorCamera.SetPosition(pos);
        }

        LastMousePos = {io.MousePos.x, io.MousePos.y};
    }

    void FViewportPanel::RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight) {
        FWorldRenderer* WorldRenderer = InWorld.GetWorldRenderer();
        if (!WorldRenderer) {
            return;
        }

        if (InWidth != LastWidth || InHeight != LastHeight) {
            WorldRenderer->OnViewportResize(InWidth, InHeight);
            EditorCamera.SetViewportSize(InWidth, InHeight);
            LastWidth = InWidth;
            LastHeight = InHeight;
        }

        WorldRenderer->Render(EditorCamera);
    }

    void FViewportPanel::Draw(UWorld* InWorld, const std::string& InMapName, AActor* InSelectedActor) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        bIsViewportFocused = ImGui::IsWindowFocused();
        bIsViewportHovered = ImGui::IsWindowHovered();

        const ImVec2 Content = ImGui::GetContentRegionAvail();
        const uint32_t Width = static_cast<uint32_t>(std::max(Content.x, 1.0f));
        const uint32_t Height = static_cast<uint32_t>(std::max(Content.y, 1.0f));

        EnsureCamera();
        ProcessCameraInput();

        bool bRenderedImage = false;

        if (InWorld) {
            RenderWorld(*InWorld, Width, Height);

            if (FWorldRenderer* WorldRenderer = InWorld->GetWorldRendererIfInitialized()) {
                if (TRef<FFramebuffer> Hdr = WorldRenderer->GetHDRSceneFramebuffer()) {
                    const uint32_t TexId = Hdr->GetColorAttachmentRendererID(0);
                    if (TexId != 0) {
                        ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(TexId)), Content, ImVec2(0, 1),
                                     ImVec2(1, 0));
                        bRenderedImage = true;
                    }
                }
            }
        }

        if (!bRenderedImage) {
            ImGui::Dummy(Content);
        }

        // Viewport Overlay Stats
        ImVec2 overlayPos = ImGui::GetWindowPos();
        ImGui::SetNextWindowPos(ImVec2(overlayPos.x + 12.0f, overlayPos.y + 36.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);

        ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("##ViewportOverlay", nullptr, overlayFlags)) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Level: %s",
                               InMapName.empty() ? "Untitled" : InMapName.c_str());
            ImGui::Separator();
            ImGui::Text("Actors: %zu", InWorld ? InWorld->GetAllActors().size() : 0);
            ImGui::Text("Cam Speed: %.1f (RMB + WASD)", CameraSpeed);
            if (InSelectedActor) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Selected: %s", InSelectedActor->GetName().c_str());
            }
        }
        ImGui::End();

        ImGui::End();
        ImGui::PopStyleVar();
    }

} // namespace Leon::Editor
