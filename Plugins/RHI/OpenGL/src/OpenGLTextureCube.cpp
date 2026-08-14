#include "OpenGLTextureCube.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"

#include <stb_image.h>

namespace Leon {

    FOpenGLTextureCube::FOpenGLTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR)
        : m_Width(InWidth), m_Height(InHeight) {
        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

        GLenum internalFormat = InbHDR ? GL_RGBA16F : GL_RGBA8;
        GLenum dataFormat = GL_RGBA;
        GLenum dataType = InbHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;
        size_t bpp = InbHDR ? 8 : 4;

        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * bpp * 6);

        for (unsigned int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, m_Width, m_Height, 0, dataFormat,
                         dataType, nullptr);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        m_IsLoaded = true;

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTextureCube::FOpenGLTextureCube(const std::vector<std::string>& InFacePaths) {
        if (InFacePaths.size() != 6) {
            LE_CORE_ERROR("FOpenGLTextureCube: Exactly 6 face texture paths are required!");
            return;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

        stbi_set_flip_vertically_on_load(0); // Cubemap standard OpenGL orientation

        int width = 0, height = 0, channels = 0;
        size_t totalBytes = 0;

        for (unsigned int i = 0; i < 6; ++i) {
            unsigned char* data = stbi_load(InFacePaths[i].c_str(), &width, &height, &channels, 4);
            if (data) {
                if (i == 0) {
                    m_Width = static_cast<uint32_t>(width);
                    m_Height = static_cast<uint32_t>(height);
                }
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
                totalBytes += static_cast<size_t>(width * height * 4);
            } else {
                LE_CORE_ERROR("FOpenGLTextureCube: Failed to load cubemap face from: {0}", InFacePaths[i]);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        m_IsLoaded = true;

        m_AllocatedBytes = totalBytes;
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTextureCube::~FOpenGLTextureCube() {
        if (m_RendererID) {
            FRenderer::OnGPUFree(m_AllocatedBytes);
            glDeleteTextures(1, &m_RendererID);
            m_RendererID = 0;
        }
    }

    void FOpenGLTextureCube::Bind(uint32_t InSlot) const {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    }

    void FOpenGLTextureCube::Unbind() const {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    void FOpenGLTextureCube::SetFaceData(uint32_t InFaceIndex, const void* InData, uint32_t InWidth,
                                         uint32_t InHeight, uint32_t InMipLevel, bool InbHDR) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
        GLenum internalFormat = InbHDR ? GL_RGBA16F : GL_RGBA8;
        GLenum dataFormat = GL_RGBA;
        GLenum dataType = InbHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + InFaceIndex, static_cast<GLint>(InMipLevel), internalFormat,
                     static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), 0, dataFormat, dataType, InData);
    }

    void FOpenGLTextureCube::GenerateMipmaps() {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }

} // namespace Leon
