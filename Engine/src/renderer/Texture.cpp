#include "renderer/Texture.hpp"
#include "core/Log.hpp"
#include "renderer/RenderDriver.hpp"
#include "renderer/TextureCube.hpp"

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

    TRef<FTextureCube> FTextureCube::Create(uint32_t InWidth, uint32_t InHeight, bool InbHDR) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for TextureCube creation!");
            return nullptr;
        }
        return driver->CreateTextureCube(InWidth, InHeight, InbHDR);
    }

    TRef<FTextureCube> FTextureCube::Create(const std::vector<std::string>& InFacePaths) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for TextureCube creation!");
            return nullptr;
        }
        return driver->CreateTextureCube(InFacePaths);
    }

} // namespace Leon
