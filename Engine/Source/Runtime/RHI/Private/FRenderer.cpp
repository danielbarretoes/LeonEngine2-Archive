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
        OnGPUAlloc(SwapchainBytes, EGPUMemoryCategory::Swapchain);
    }

    void FRenderer::Shutdown() {
        LE_CORE_INFO("Shutting down Renderer Subsystem...");
        OnGPUFree(SwapchainBytes, EGPUMemoryCategory::Swapchain);
        SwapchainBytes = 0;
        FRenderCommand::Shutdown();
    }

    void FRenderer::OnWindowResize(unsigned int InWidth, unsigned int InHeight) {
        FRenderCommand::SetViewport(0, 0, InWidth, InHeight);

        OnGPUFree(SwapchainBytes, EGPUMemoryCategory::Swapchain);
        SwapchainBytes = static_cast<size_t>(InWidth * InHeight * 12);
        OnGPUAlloc(SwapchainBytes, EGPUMemoryCategory::Swapchain);
    }

    void FRenderer::OnGPUAlloc(size_t InBytes, EGPUMemoryCategory InCategory, const char* InLabel) {
        if (InBytes == 0)
            return;

        Stats.AllocatedGPUMemoryBytes += InBytes;

        const size_t index = static_cast<size_t>(InCategory);
        if (index < static_cast<size_t>(EGPUMemoryCategory::Count)) {
            Stats.GPUMemory[index].Bytes += InBytes;
            Stats.GPUMemory[index].Allocations++;
        }

        if (InLabel && InLabel[0] != '\0') {
            auto& bucket = Stats.NamedGPUMemory[InLabel];
            bucket.Bytes += InBytes;
            bucket.Allocations++;
        }
    }

    void FRenderer::OnGPUFree(size_t InBytes, EGPUMemoryCategory InCategory, const char* InLabel) {
        if (InBytes == 0)
            return;

        if (Stats.AllocatedGPUMemoryBytes >= InBytes)
            Stats.AllocatedGPUMemoryBytes -= InBytes;
        else
            Stats.AllocatedGPUMemoryBytes = 0;

        const size_t index = static_cast<size_t>(InCategory);
        if (index < static_cast<size_t>(EGPUMemoryCategory::Count)) {
            auto& category = Stats.GPUMemory[index];
            category.Bytes = category.Bytes >= InBytes ? category.Bytes - InBytes : 0;
            if (category.Allocations > 0)
                category.Allocations--;
        }

        if (InLabel && InLabel[0] != '\0') {
            auto it = Stats.NamedGPUMemory.find(InLabel);
            if (it != Stats.NamedGPUMemory.end()) {
                auto& bucket = it->second;
                bucket.Bytes = bucket.Bytes >= InBytes ? bucket.Bytes - InBytes : 0;
                if (bucket.Allocations > 0)
                    bucket.Allocations--;
                if (bucket.Bytes == 0 && bucket.Allocations == 0)
                    Stats.NamedGPUMemory.erase(it);
            }
        }
    }

} // namespace Leon
