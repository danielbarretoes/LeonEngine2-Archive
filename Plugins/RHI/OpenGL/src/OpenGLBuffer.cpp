#include "OpenGLBuffer.hpp"
#include "renderer/Renderer.hpp"
#include <glad/glad.h>

namespace Leon {

    // VertexBuffer
    FOpenGLVertexBuffer::FOpenGLVertexBuffer(unsigned int InSize) : m_AllocatedBytes(InSize) {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferData(m_RendererID, InSize, nullptr, GL_DYNAMIC_DRAW);
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLVertexBuffer::FOpenGLVertexBuffer(const float* InVertices, unsigned int InSize) : m_AllocatedBytes(InSize) {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferStorage(m_RendererID, InSize, InVertices, 0);
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLVertexBuffer::~FOpenGLVertexBuffer() {
        FRenderer::OnGPUFree(m_AllocatedBytes);
        glDeleteBuffers(1, &m_RendererID);
    }

    void FOpenGLVertexBuffer::Bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    }

    void FOpenGLVertexBuffer::Unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void FOpenGLVertexBuffer::SetData(const void* InData, unsigned int InSize) {
        glNamedBufferSubData(m_RendererID, 0, InSize, InData);
    }

    // IndexBuffer
    FOpenGLIndexBuffer::FOpenGLIndexBuffer(const uint32_t* InIndices, unsigned int InCount)
        : m_Count(InCount), m_AllocatedBytes(InCount * sizeof(uint32_t)) {
        glCreateBuffers(1, &m_RendererID);
        glNamedBufferStorage(m_RendererID, m_AllocatedBytes, InIndices, 0);
        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLIndexBuffer::~FOpenGLIndexBuffer() {
        FRenderer::OnGPUFree(m_AllocatedBytes);
        glDeleteBuffers(1, &m_RendererID);
    }

    void FOpenGLIndexBuffer::Bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    }

    void FOpenGLIndexBuffer::Unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace Leon
