#include "opengl/OpenGLBuffer.hpp"
#include <glad/glad.h>

namespace Leon {

    // VertexBuffer
    FOpenGLVertexBuffer::FOpenGLVertexBuffer(unsigned int InSize) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, InSize, nullptr, GL_DYNAMIC_DRAW);
    }

    FOpenGLVertexBuffer::FOpenGLVertexBuffer(const float* InVertices, unsigned int InSize) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ARRAY_BUFFER, InSize, InVertices, GL_STATIC_DRAW);
    }

    FOpenGLVertexBuffer::~FOpenGLVertexBuffer() {
        glDeleteBuffers(1, &m_RendererID);
    }

    void FOpenGLVertexBuffer::Bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    }

    void FOpenGLVertexBuffer::Unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void FOpenGLVertexBuffer::SetData(const void* InData, unsigned int InSize) {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, InSize, InData);
    }

    // IndexBuffer
    FOpenGLIndexBuffer::FOpenGLIndexBuffer(const uint32_t* InIndices, unsigned int InCount) : m_Count(InCount) {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, InCount * sizeof(uint32_t), InIndices, GL_STATIC_DRAW);
    }

    FOpenGLIndexBuffer::~FOpenGLIndexBuffer() {
        glDeleteBuffers(1, &m_RendererID);
    }

    void FOpenGLIndexBuffer::Bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    }

    void FOpenGLIndexBuffer::Unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace Leon
