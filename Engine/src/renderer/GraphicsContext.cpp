#include "renderer/GraphicsContext.hpp"
#include "renderer/RenderDriver.hpp"

namespace Leon {

    TScope<IGraphicsContext> IGraphicsContext::Create(void* InWindowHandle) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateGraphicsContext(InWindowHandle);
        }
        return nullptr;
    }

} // namespace Leon
