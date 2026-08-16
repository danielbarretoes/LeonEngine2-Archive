#include "FOpenGLBuffer.hpp"
#include "RHI/FRenderer.hpp"
#include <glad/glad.h>

namespace Leon {

    // FVertexBuffer
    FOpenGLVertexBuffer::FOpenGLVertexBuffer(unsigned int InSize) : AllocatedBytes(InSize) {
        glCreateBuffers(1, &RendererID);
        glNamedBufferData(RendererID, InSize, nullptr, GL_DYNAMIC_DRAW);
        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    FOpenGLVertexBuffer::FOpenGLVertexBuffer(const float* InVertices, unsigned int InSize) : AllocatedBytes(InSize) {
        glCreateBuffers(1, &RendererID);
        glNamedBufferStorage(RendererID, InSize, InVertices, 0);
        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    FOpenGLVertexBuffer::~FOpenGLVertexBuffer() {
        FRenderer::OnGPUFree(AllocatedBytes);
        glDeleteBuffers(1, &RendererID);
    }

    void FOpenGLVertexBuffer::Bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, RendererID);
    }

    void FOpenGLVertexBuffer::Unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void FOpenGLVertexBuffer::SetData(const void* InData, unsigned int InSize) {
        glNamedBufferSubData(RendererID, 0, InSize, InData);
    }

    // FIndexBuffer
    FOpenGLIndexBuffer::FOpenGLIndexBuffer(const uint32_t* InIndices, unsigned int InCount)
        : Count(InCount), AllocatedBytes(InCount * sizeof(uint32_t)) {
        glCreateBuffers(1, &RendererID);
        glNamedBufferStorage(RendererID, AllocatedBytes, InIndices, 0);
        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    FOpenGLIndexBuffer::~FOpenGLIndexBuffer() {
        FRenderer::OnGPUFree(AllocatedBytes);
        glDeleteBuffers(1, &RendererID);
    }

    void FOpenGLIndexBuffer::Bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RendererID);
    }

    void FOpenGLIndexBuffer::Unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace Leon
