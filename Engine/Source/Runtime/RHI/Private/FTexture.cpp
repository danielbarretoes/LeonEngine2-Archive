#include "RHI/FTexture.hpp"
#include "Core/FLog.hpp"
#include "RHI/IRenderDriver.hpp"
#include "RHI/FTextureCube.hpp"

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

    TRef<FTexture2D> FTexture2D::CreateWithFormat(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat) {
        IRenderDriver* driver = FRenderDriverRegistry::GetActiveDriver();
        if (!driver) {
            LE_CORE_ASSERT(false, "No active RenderDriver registered for Texture2D creation!");
            return nullptr;
        }
        return driver->CreateTexture2DWithFormat(InWidth, InHeight, InFormat);
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
