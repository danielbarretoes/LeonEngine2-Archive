#include "RHI/FRenderer.hpp"
#include "Core/FLog.hpp"

namespace Leon {

    FRenderStats FRenderer::Stats;
    static size_t SwapchainBytes = 0;

    void FRenderer::Init() {
        LE_CORE_INFO("Initializing Renderer Subsystem...");
        FRenderCommand::Init();

        // Default swapchain allocation accounting: Front Color (4) + Back Color (4) + Depth/Stencil (4) = 12 bytes/px
        SwapchainBytes = 1280 * 720 * 12;
        OnGPUAlloc(SwapchainBytes);
    }

    void FRenderer::Shutdown() {
        LE_CORE_INFO("Shutting down Renderer Subsystem...");
        OnGPUFree(SwapchainBytes);
        SwapchainBytes = 0;
    }

    void FRenderer::OnWindowResize(unsigned int InWidth, unsigned int InHeight) {
        FRenderCommand::SetViewport(0, 0, InWidth, InHeight);

        OnGPUFree(SwapchainBytes);
        SwapchainBytes = static_cast<size_t>(InWidth * InHeight * 12);
        OnGPUAlloc(SwapchainBytes);
    }

} // namespace Leon
