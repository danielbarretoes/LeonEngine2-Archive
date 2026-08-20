#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

namespace Leon::Editor {

/**
 * Level viewport panel: docks into ImGui and displays FWorldRenderer output.
 * Empty worlds still clear through the HDR scene framebuffer when available.
 */
class FViewportPanel {
public:
    void Draw(UWorld* InWorld);

private:
    void EnsureCamera();
    void RenderWorld(UWorld& InWorld, uint32_t InWidth, uint32_t InHeight);

    FPerspectiveCamera EditorCamera{45.0f, 1.778f, 0.1f, 1000.0f};
    bool bCameraReady = false;
    uint32_t LastWidth = 0;
    uint32_t LastHeight = 0;
};

} // namespace Leon::Editor
