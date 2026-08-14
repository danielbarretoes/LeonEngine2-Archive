#include "OpenGLTexture2D.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"
#include <stb_image.h>

namespace Leon {

    // -------------------------------------------------------------------------
    // Blank RGBA8 texture (used for render target / manual SetData uploads)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight)
        : m_Width(InWidth), m_Height(InHeight) {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat     = GL_RGBA;
        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                     m_DataFormat, GL_UNSIGNED_BYTE, nullptr);
        m_IsLoaded = true;

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    // -------------------------------------------------------------------------
    // RG16F float texture (used for BRDF LUT — needs half-float precision)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat) {
        m_Width  = InWidth;
        m_Height = InHeight;

        switch (InFormat) {
            case ETextureFormat::RG16F:
                m_InternalFormat = GL_RG16F;
                m_DataFormat     = GL_RG;
                m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 2 * sizeof(uint16_t));
                break;
            case ETextureFormat::RGBA16F:
                m_InternalFormat = GL_RGBA16F;
                m_DataFormat     = GL_RGBA;
                m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4 * sizeof(uint16_t));
                break;
            case ETextureFormat::RGBA32F:
                m_InternalFormat = GL_RGBA32F;
                m_DataFormat     = GL_RGBA;
                m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4 * sizeof(float));
                break;
            default: // RGBA8
                m_InternalFormat = GL_RGBA8;
                m_DataFormat     = GL_RGBA;
                m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4);
                break;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                     m_DataFormat, GL_FLOAT, nullptr);
        m_IsLoaded = true;

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    // -------------------------------------------------------------------------
    // File-loaded texture (LDR or HDR)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(const std::string& InPath) : m_Path(InPath) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        if (stbi_is_hdr(InPath.c_str())) {
            // HDR equirectangular map — load raw, no preprocessing
            float* data = stbi_loadf(InPath.c_str(), &width, &height, &channels, 4);
            if (!data) {
                LE_CORE_ERROR("Failed to load HDR texture from: {0}", InPath);
                return;
            }

            m_IsLoaded       = true;
            m_Width          = static_cast<uint32_t>(width);
            m_Height         = static_cast<uint32_t>(height);
            m_InternalFormat = GL_RGBA32F;
            m_DataFormat     = GL_RGBA;
            m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4 * sizeof(float));

            glGenTextures(1, &m_RendererID);
            glBindTexture(GL_TEXTURE_2D, m_RendererID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                         m_DataFormat, GL_FLOAT, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(data);
            FRenderer::OnGPUAlloc(m_AllocatedBytes);

            LE_CORE_INFO("Loaded HDR Environment Map: {0} ({1}x{2}, RGBA32F, {3} KB)",
                         InPath, m_Width, m_Height, m_AllocatedBytes / 1024);
            return;
        }

        // Standard LDR texture (PNG, JPG, BMP, TGA)
        stbi_uc* data = stbi_load(InPath.c_str(), &width, &height, &channels, 0);
        if (!data) {
            LE_CORE_ERROR("Failed to load texture from: {0}", InPath);
            return;
        }

        m_IsLoaded = true;
        m_Width    = static_cast<uint32_t>(width);
        m_Height   = static_cast<uint32_t>(height);

        uint32_t bpp = 4;
        if (channels == 4) {
            m_InternalFormat = GL_RGBA8;
            m_DataFormat     = GL_RGBA;
            bpp = 4;
        } else if (channels == 3) {
            m_InternalFormat = GL_RGB8;
            m_DataFormat     = GL_RGB;
            bpp = 3;
        } else if (channels == 1) {
            m_InternalFormat = GL_R8;
            m_DataFormat     = GL_RED;
            bpp = 1;
        } else {
            LE_CORE_ERROR("Texture format not supported for: {0} ({1} channels)", InPath, channels);
            stbi_image_free(data);
            return;
        }

        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * bpp);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                     m_DataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        stbi_image_free(data);
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTexture2D::~FOpenGLTexture2D() {
        if (m_RendererID) {
            FRenderer::OnGPUFree(m_AllocatedBytes);
            glDeleteTextures(1, &m_RendererID);
        }
    }

    void FOpenGLTexture2D::SetData(void* InData, uint32_t InSize) {
        uint32_t bpp = (m_DataFormat == GL_RGBA) ? 4 : (m_DataFormat == GL_RGB ? 3 : (m_DataFormat == GL_RG ? 2 : 1));
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, InData);
        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    void FOpenGLTexture2D::SetDataFloat(const void* InData, uint32_t /*InSize*/) {
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_FLOAT, InData);
    }

    void FOpenGLTexture2D::Bind(uint32_t InSlot) const {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

} // namespace Leon
