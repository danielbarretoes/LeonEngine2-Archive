#include "OpenGLTextureCube.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"

#include <stb_image.h>
#include <cmath>
#include <algorithm>

namespace Leon {

    static uint32_t CalculateCubemapMipLevels(uint32_t InWidth, uint32_t InHeight) {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(InWidth, InHeight)))) + 1;
    }

    FOpenGLTextureCube::FOpenGLTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR)
        : m_Width(InWidth), m_Height(InHeight) {
        GLenum internalFormat = InbHDR ? GL_RGBA16F : GL_RGBA8;
        size_t bpp = InbHDR ? 8 : 4;

        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * bpp * 6);
        uint32_t levels = CalculateCubemapMipLevels(m_Width, m_Height);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, levels, internalFormat, m_Width, m_Height);

        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAX_LEVEL, 0);

        m_IsLoaded = true;
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTextureCube::FOpenGLTextureCube(const std::vector<std::string>& InFacePaths) {
        if (InFacePaths.size() != 6) {
            LE_CORE_ERROR("FOpenGLTextureCube: Exactly 6 face texture paths are required!");
            return;
        }

        stbi_set_flip_vertically_on_load(0); // Cubemap standard OpenGL orientation

        int width = 0, height = 0, channels = 0;
        size_t totalBytes = 0;
        unsigned char* faceData[6] = {nullptr};

        for (unsigned int i = 0; i < 6; ++i) {
            faceData[i] = stbi_load(InFacePaths[i].c_str(), &width, &height, &channels, 4);
            if (!faceData[i]) {
                LE_CORE_ERROR("FOpenGLTextureCube: Failed to load cubemap face from: {0}", InFacePaths[i]);
            }
        }

        if (width > 0 && height > 0) {
            m_Width = static_cast<uint32_t>(width);
            m_Height = static_cast<uint32_t>(height);
            totalBytes = static_cast<size_t>(m_Width * m_Height * 4 * 6);
            uint32_t levels = CalculateCubemapMipLevels(m_Width, m_Height);

            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
            glTextureStorage2D(m_RendererID, levels, GL_RGBA8, m_Width, m_Height);

            for (unsigned int i = 0; i < 6; ++i) {
                if (faceData[i]) {
                    glTextureSubImage3D(m_RendererID, 0, 0, 0, i, m_Width, m_Height, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                        faceData[i]);
                    stbi_image_free(faceData[i]);
                }
            }

            glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTextureParameteri(m_RendererID, GL_TEXTURE_BASE_LEVEL, 0);

            GenerateMipmaps();
            m_IsLoaded = true;

            m_AllocatedBytes = totalBytes;
            FRenderer::OnGPUAlloc(m_AllocatedBytes);
        }
    }

    FOpenGLTextureCube::~FOpenGLTextureCube() {
        if (m_RendererID) {
            FRenderer::OnGPUFree(m_AllocatedBytes);
            glDeleteTextures(1, &m_RendererID);
            m_RendererID = 0;
        }
    }

    void FOpenGLTextureCube::Bind(uint32_t InSlot) const {
        glBindTextureUnit(InSlot, m_RendererID);
    }

    void FOpenGLTextureCube::Unbind() const {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    void FOpenGLTextureCube::SetFaceData(uint32_t InFaceIndex, const void* InData, uint32_t InWidth, uint32_t InHeight,
                                         uint32_t InMipLevel, bool InbHDR) {
        GLenum dataFormat = GL_RGBA;
        GLenum dataType = InbHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;

        glTextureSubImage3D(m_RendererID, static_cast<GLint>(InMipLevel), 0, 0, static_cast<GLint>(InFaceIndex),
                            static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), 1, dataFormat, dataType,
                            InData);

        if (InMipLevel > m_MaxMipLevel) {
            m_MaxMipLevel = InMipLevel;
            glTextureParameteri(m_RendererID, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(m_MaxMipLevel));
            glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
    }

    void FOpenGLTextureCube::GenerateMipmaps() {
        glGenerateTextureMipmap(m_RendererID);
        m_MaxMipLevel = CalculateCubemapMipLevels(m_Width, m_Height) - 1;
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(m_MaxMipLevel));
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

} // namespace Leon
