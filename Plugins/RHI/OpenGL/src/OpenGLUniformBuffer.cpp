#include "OpenGLUniformBuffer.hpp"
#include "renderer/Renderer.hpp"

namespace Leon {

    FOpenGLUniformBuffer::FOpenGLUniformBuffer(uint32_t InSize, uint32_t InBinding)
        : m_Binding(InBinding), m_AllocatedBytes(InSize) {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferData(m_RendererID, InSize, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, InBinding, m_RendererID);

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLUniformBuffer::~FOpenGLUniformBuffer() {
        if (m_RendererID) {
            FRenderer::OnGPUFree(m_AllocatedBytes);
            glDeleteBuffers(1, &m_RendererID);
        }
    }

    void FOpenGLUniformBuffer::SetData(const void* InData, uint32_t InSize, uint32_t InOffset) {
        glNamedBufferSubData(m_RendererID, InOffset, InSize, InData);
    }

} // namespace Leon
