#pragma once

#include "renderer/Texture.hpp"
#include <glad/glad.h>
#include <cstdint>

namespace Leon {

    class FOpenGLTexture2D : public FTexture2D {
    public:
        /// Blank RGBA8 texture for manual SetData uploads
        FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight);

        /// Explicit format texture for float precision (BRDF LUT, offscreen HDR targets)
        FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat);

        /// File-loaded texture (LDR: PNG/JPG/TGA, or HDR: .hdr equirectangular)
        FOpenGLTexture2D(const std::string& InPath);

        ~FOpenGLTexture2D() override;

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }
        uint32_t GetRendererID() const override { return m_RendererID; }
        const std::string& GetPath() const override { return m_Path; }

        void SetData(void* InData, uint32_t InSize) override;

        /// Upload raw float data (for RG16F / RGBA16F / RGBA32F textures)
        void SetDataFloat(const void* InData, uint32_t InSize) override;

        void Bind(uint32_t InSlot = 0) const override;
        bool IsLoaded() const override { return m_IsLoaded; }

        bool operator==(const FTexture& InOther) const override { return m_RendererID == InOther.GetRendererID(); }

    private:
        std::string m_Path;
        bool m_IsLoaded = false;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_RendererID = 0;
        size_t m_AllocatedBytes = 0;
        GLenum m_InternalFormat = 0;
        GLenum m_DataFormat = 0;
    };

    using OpenGLTexture2D = FOpenGLTexture2D;

} // namespace Leon
