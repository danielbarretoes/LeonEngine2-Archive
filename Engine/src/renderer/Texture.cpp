#include "engine/renderer/Texture.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/RenderDriver.hpp"

namespace Leon {

    TRef<FTexture2D> FTexture2D::Create(uint32_t InWidth, uint32_t InHeight) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for Texture2D creation!");
            return nullptr;
        }
        return driver->CreateTexture2D(InWidth, InHeight);
    }

    TRef<FTexture2D> FTexture2D::Create(const std::string& InPath) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for Texture2D creation!");
            return nullptr;
        }
        return driver->CreateTexture2D(InPath);
    }

} // namespace Leon
