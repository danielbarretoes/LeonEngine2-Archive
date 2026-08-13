#include "opengl/OpenGLTexture2D.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/Renderer.hpp"
#include <stb_image.h>

namespace Leon {

    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight) : m_Width(InWidth), m_Height(InHeight) {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat = GL_RGBA;
        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);
        m_IsLoaded = true;

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTexture2D::FOpenGLTexture2D(const std::string& InPath) : m_Path(InPath) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);
        stbi_uc* data = stbi_load(InPath.c_str(), &width, &height, &channels, 0);

        if (!data) {
            LE_CORE_ERROR("Failed to load texture image from: {0}", InPath);
            return;
        }

        m_IsLoaded = true;
        m_Width = static_cast<uint32_t>(width);
        m_Height = static_cast<uint32_t>(height);

        GLenum internalFormat = 0, dataFormat = 0;
        uint32_t bpp = 4;
        if (channels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            bpp = 4;
        } else if (channels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
            bpp = 3;
        } else if (channels == 1) {
            internalFormat = GL_R8;
            dataFormat = GL_RED;
            bpp = 1;
        }

        m_InternalFormat = internalFormat;
        m_DataFormat = dataFormat;
        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * bpp);

        if (internalFormat == 0 || dataFormat == 0) {
            LE_CORE_ERROR("Texture format not supported for: {0} ({1} channels)", InPath, channels);
            stbi_image_free(data);
            return;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

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
        uint32_t bpp = (m_DataFormat == GL_RGBA) ? 4 : (m_DataFormat == GL_RGB ? 3 : 1);
        LE_CORE_ASSERT(InSize == m_Width * m_Height * bpp, "Data must be entire texture!");
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, InData);
    }

    void FOpenGLTexture2D::Bind(uint32_t InSlot) const {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

} // namespace Leon
