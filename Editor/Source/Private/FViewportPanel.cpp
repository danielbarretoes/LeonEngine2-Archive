#include "Editor/FViewportPanel.hpp"

#include "RHI/FFramebuffer.hpp"
#include "Renderer/FWorldRenderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace Leon::Editor {

void FViewportPanel::EnsureCamera() {
    if (bCameraReady) {
        return;
    }
    EditorCamera.SetPosition({0.0f, 2.0f, 6.0f});
    EditorCamera.SetRotation(-15.0f, -90.0f);
    bCameraReady = true;
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

void FViewportPanel::Draw(UWorld* InWorld) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");

    const ImVec2 Content = ImGui::GetContentRegionAvail();
    const uint32_t Width = static_cast<uint32_t>(std::max(Content.x, 1.0f));
    const uint32_t Height = static_cast<uint32_t>(std::max(Content.y, 1.0f));

    EnsureCamera();

    if (InWorld) {
        RenderWorld(*InWorld, Width, Height);

        if (FWorldRenderer* WorldRenderer = InWorld->GetWorldRendererIfInitialized()) {
            if (TRef<FFramebuffer> Hdr = WorldRenderer->GetHDRSceneFramebuffer()) {
                const uint32_t TexId = Hdr->GetColorAttachmentRendererID(0);
                if (TexId != 0) {
                    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(TexId)), Content, ImVec2(0, 1),
                                 ImVec2(1, 0));
                    ImGui::End();
                    ImGui::PopStyleVar();
                    return;
                }
            }
        }
    }

    ImGui::Dummy(Content);
    const ImVec2 Cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(Cursor.x + 16.0f, Cursor.y - Content.y + 16.0f));
    ImGui::TextUnformatted("Viewport — empty UWorld (skeleton)");

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Leon::Editor
