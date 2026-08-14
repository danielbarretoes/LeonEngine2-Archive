#pragma once

#include "renderer/Buffer.hpp"
#include <glad/glad.h>

namespace Leon {

    class FOpenGLUniformBuffer : public FUniformBuffer {
    public:
        FOpenGLUniformBuffer(uint32_t InSize, uint32_t InBinding);
        ~FOpenGLUniformBuffer() override;

        void SetData(const void* InData, uint32_t InSize, uint32_t InOffset = 0) override;
        uint32_t GetBinding() const override { return m_Binding; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Binding = 0;
        size_t m_AllocatedBytes = 0;
    };

    using OpenGLUniformBuffer = FOpenGLUniformBuffer;

} // namespace Leon
