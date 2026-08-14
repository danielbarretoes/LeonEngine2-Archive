#include "renderer/Framebuffer.hpp"
#include "core/Log.hpp"
#include "renderer/RenderDriver.hpp"

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
