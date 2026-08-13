#pragma once

#include "engine/renderer/Buffer.hpp"

namespace Leon {

    class FOpenGLVertexBuffer : public FVertexBuffer {
    public:
        FOpenGLVertexBuffer(unsigned int InSize);
        FOpenGLVertexBuffer(const float* InVertices, unsigned int InSize);
        ~FOpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        void SetData(const void* InData, unsigned int InSize) override;
        const FBufferLayout& GetLayout() const override { return m_Layout; }
        void SetLayout(const FBufferLayout& InLayout) override { m_Layout = InLayout; }

    private:
        unsigned int m_RendererID = 0;
        size_t m_AllocatedBytes = 0;
        FBufferLayout m_Layout;
    };

    using OpenGLVertexBuffer = FOpenGLVertexBuffer;

    class FOpenGLIndexBuffer : public FIndexBuffer {
    public:
        FOpenGLIndexBuffer(const uint32_t* InIndices, unsigned int InCount);
        ~FOpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        unsigned int GetCount() const override { return m_Count; }

    private:
        unsigned int m_RendererID = 0;
        unsigned int m_Count = 0;
        size_t m_AllocatedBytes = 0;
    };

    using OpenGLIndexBuffer = FOpenGLIndexBuffer;

} // namespace Leon
