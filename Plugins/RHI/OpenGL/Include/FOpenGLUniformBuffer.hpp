#pragma once

#include "RHI/FBuffer.hpp"
#include <glad/glad.h>

namespace Leon {

    class FOpenGLUniformBuffer : public FUniformBuffer {
    public:
        FOpenGLUniformBuffer(uint32_t InSize, uint32_t InBinding);
        ~FOpenGLUniformBuffer() override;

        void SetData(const void* InData, uint32_t InSize, uint32_t InOffset = 0) override;
        uint32_t GetBinding() const override { return Binding; }

    private:
        uint32_t RendererID = 0;
        uint32_t Binding = 0;
        size_t AllocatedBytes = 0;
    };

} // namespace Leon
