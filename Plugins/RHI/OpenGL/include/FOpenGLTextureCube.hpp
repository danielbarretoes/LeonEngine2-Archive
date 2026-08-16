#pragma once

#include "RHI/FTextureCube.hpp"
#include <glad/glad.h>
#include <string>
#include <vector>

namespace Leon {

    class FOpenGLTextureCube : public FTextureCube {
    public:
        FOpenGLTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR = false);
        FOpenGLTextureCube(const std::vector<std::string>& InFacePaths);
        ~FOpenGLTextureCube() override;

        uint32_t GetWidth() const override { return Width; }
        uint32_t GetHeight() const override { return Height; }
        uint32_t GetRendererID() const override { return RendererID; }

        void Bind(uint32_t InSlot = 0) const override;
        void Unbind() const override;

        bool IsLoaded() const override { return bIsLoaded; }

        void SetFaceData(uint32_t InFaceIndex, const void* InData, uint32_t InWidth, uint32_t InHeight,
                         uint32_t InMipLevel = 0, bool InbHDR = false) override;
        void GenerateMipmaps() override;

    private:
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t RendererID = 0;
        uint32_t MaxMipLevel = 0;
        bool bIsLoaded = false;
        size_t AllocatedBytes = 0;
    };

} // namespace Leon
