#include "FOpenGLTextureCube.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderer.hpp"

#include <stb_image.h>
#include <cmath>
#include <algorithm>

namespace Leon {

    static uint32_t CalculateCubemapMipLevels(uint32_t InWidth, uint32_t InHeight) {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(InWidth, InHeight)))) + 1;
    }

    FOpenGLTextureCube::FOpenGLTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR)
        : Width(InWidth), Height(InHeight) {
        GLenum internalFormat = InbHDR ? GL_RGBA16F : GL_RGBA8;
        size_t bpp = InbHDR ? 8 : 4;

        AllocatedBytes = static_cast<size_t>(Width * Height * bpp * 6);
        uint32_t levels = CalculateCubemapMipLevels(Width, Height);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &RendererID);
        glTextureStorage2D(RendererID, levels, internalFormat, Width, Height);

        glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(RendererID, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(RendererID, GL_TEXTURE_MAX_LEVEL, 0);

        bIsLoaded = true;
        FRenderer::OnGPUAlloc(AllocatedBytes);
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
            Width = static_cast<uint32_t>(width);
            Height = static_cast<uint32_t>(height);
            totalBytes = static_cast<size_t>(Width * Height * 4 * 6);
            uint32_t levels = CalculateCubemapMipLevels(Width, Height);

            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &RendererID);
            glTextureStorage2D(RendererID, levels, GL_RGBA8, Width, Height);

            for (unsigned int i = 0; i < 6; ++i) {
                if (faceData[i]) {
                    glTextureSubImage3D(RendererID, 0, 0, 0, i, Width, Height, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                        faceData[i]);
                    stbi_image_free(faceData[i]);
                }
            }

            glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTextureParameteri(RendererID, GL_TEXTURE_BASE_LEVEL, 0);

            GenerateMipmaps();
            bIsLoaded = true;

            AllocatedBytes = totalBytes;
            FRenderer::OnGPUAlloc(AllocatedBytes);
        }
    }

    FOpenGLTextureCube::~FOpenGLTextureCube() {
        if (RendererID) {
            FRenderer::OnGPUFree(AllocatedBytes);
            glDeleteTextures(1, &RendererID);
            RendererID = 0;
        }
    }

    void FOpenGLTextureCube::Bind(uint32_t InSlot) const {
        glBindTextureUnit(InSlot, RendererID);
        FRenderer::GetStatsMutable().TextureBinds++;
    }

    void FOpenGLTextureCube::Unbind() const {
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    void FOpenGLTextureCube::SetFaceData(uint32_t InFaceIndex, const void* InData, uint32_t InWidth, uint32_t InHeight,
                                         uint32_t InMipLevel, bool InbHDR) {
        GLenum dataFormat = GL_RGBA;
        GLenum dataType = InbHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;

        glTextureSubImage3D(RendererID, static_cast<GLint>(InMipLevel), 0, 0, static_cast<GLint>(InFaceIndex),
                            static_cast<GLsizei>(InWidth), static_cast<GLsizei>(InHeight), 1, dataFormat, dataType,
                            InData);

        if (InMipLevel > MaxMipLevel) {
            MaxMipLevel = InMipLevel;
            glTextureParameteri(RendererID, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(MaxMipLevel));
            glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
    }

    void FOpenGLTextureCube::GenerateMipmaps() {
        glGenerateTextureMipmap(RendererID);
        MaxMipLevel = CalculateCubemapMipLevels(Width, Height) - 1;
        glTextureParameteri(RendererID, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(MaxMipLevel));
        glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

} // namespace Leon
