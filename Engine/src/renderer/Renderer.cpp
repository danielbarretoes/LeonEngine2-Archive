#include "renderer/Renderer.hpp"
#include "core/Log.hpp"

namespace Leon {

    FRenderStats FRenderer::s_Stats;
    static size_t s_SwapchainBytes = 0;

    void FRenderer::Init() {
        LE_CORE_INFO("Initializing Renderer Subsystem...");
        FRenderCommand::Init();

        // Default swapchain allocation accounting: Front Color (4) + Back Color (4) + Depth/Stencil (4) = 12 bytes/px
        s_SwapchainBytes = 1280 * 720 * 12;
        OnGPUAlloc(s_SwapchainBytes);
    }

    void FRenderer::Shutdown() {
        LE_CORE_INFO("Shutting down Renderer Subsystem...");
        OnGPUFree(s_SwapchainBytes);
        s_SwapchainBytes = 0;
    }

    void FRenderer::OnWindowResize(unsigned int InWidth, unsigned int InHeight) {
        FRenderCommand::SetViewport(0, 0, InWidth, InHeight);

        OnGPUFree(s_SwapchainBytes);
        s_SwapchainBytes = static_cast<size_t>(InWidth * InHeight * 12);
        OnGPUAlloc(s_SwapchainBytes);
    }

} // namespace Leon
