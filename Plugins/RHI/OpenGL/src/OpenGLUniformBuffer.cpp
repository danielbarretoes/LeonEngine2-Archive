#include "OpenGLUniformBuffer.hpp"
#include "renderer/Renderer.hpp"

namespace Leon {

    FOpenGLUniformBuffer::FOpenGLUniformBuffer(uint32_t InSize, uint32_t InBinding)
        : m_Binding(InBinding), m_AllocatedBytes(InSize) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
        glBufferData(GL_UNIFORM_BUFFER, InSize, nullptr, GL_DYNAMIC_DRAW);
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
        glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
        glBufferSubData(GL_UNIFORM_BUFFER, InOffset, InSize, InData);
    }

} // namespace Leon
