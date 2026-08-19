#include "FOpenGLUniformBuffer.hpp"
#include "RHI/FRenderer.hpp"

namespace Leon {

    FOpenGLUniformBuffer::FOpenGLUniformBuffer(uint32_t InSize, uint32_t InBinding)
        : Binding(InBinding), AllocatedBytes(InSize) {
        glCreateBuffers(1, &RendererID);
        glNamedBufferData(RendererID, InSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, InBinding, RendererID);

        FRenderer::OnGPUAlloc(AllocatedBytes, EGPUMemoryCategory::UniformBuffer);
    }

    FOpenGLUniformBuffer::~FOpenGLUniformBuffer() {
        if (RendererID) {
            FRenderer::OnGPUFree(AllocatedBytes, EGPUMemoryCategory::UniformBuffer);
            glDeleteBuffers(1, &RendererID);
        }
    }

    void FOpenGLUniformBuffer::SetData(const void* InData, uint32_t InSize, uint32_t InOffset) {
        glNamedBufferSubData(RendererID, InOffset, InSize, InData);
    }

} // namespace Leon
