#pragma once

#include "renderer/TextureCube.hpp"
#include <glad/glad.h>
#include <string>
#include <vector>

namespace Leon {

    class FOpenGLTextureCube : public FTextureCube {
    public:
        FOpenGLTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR = false);
        FOpenGLTextureCube(const std::vector<std::string>& InFacePaths);
        ~FOpenGLTextureCube() override;

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }
        uint32_t GetRendererID() const override { return m_RendererID; }

        void Bind(uint32_t InSlot = 0) const override;
        void Unbind() const override;

        bool IsLoaded() const override { return m_IsLoaded; }

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_RendererID = 0;
        bool m_IsLoaded = false;
        size_t m_AllocatedBytes = 0;
    };

    using OpenGLTextureCube = FOpenGLTextureCube;

} // namespace Leon
