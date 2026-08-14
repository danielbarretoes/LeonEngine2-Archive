#include "engine/renderer/Framebuffer.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/RenderDriver.hpp"

namespace Leon {

    TRef<FFramebuffer> FFramebuffer::Create(const FFramebufferSpecification& InSpec) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for Framebuffer creation!");
            return nullptr;
        }
        return driver->CreateFramebuffer(InSpec);
    }

} // namespace Leon
