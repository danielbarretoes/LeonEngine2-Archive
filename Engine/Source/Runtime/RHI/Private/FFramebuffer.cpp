#include "RHI/FFramebuffer.hpp"
#include "Core/FLog.hpp"
#include "RHI/IRenderDriver.hpp"

namespace Leon {

    TRef<FFramebuffer> FFramebuffer::Create(const FFramebufferSpecification& InSpec) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for FFramebuffer creation!");
            return nullptr;
        }
        return driver->CreateFramebuffer(InSpec);
    }

} // namespace Leon
