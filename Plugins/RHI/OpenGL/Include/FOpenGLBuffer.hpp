#pragma once

#include "RHI/FBuffer.hpp"

namespace Leon {

    class FOpenGLVertexBuffer : public FVertexBuffer {
    public:
        FOpenGLVertexBuffer(unsigned int InSize);
        FOpenGLVertexBuffer(const float* InVertices, unsigned int InSize);
        ~FOpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        void SetData(const void* InData, unsigned int InSize) override;
        const FBufferLayout& GetLayout() const override { return Layout; }
        void SetLayout(const FBufferLayout& InLayout) override { Layout = InLayout; }

        uint32_t GetRendererID() const { return RendererID; }

    private:
        unsigned int RendererID = 0;
        size_t AllocatedBytes = 0;
        FBufferLayout Layout;
    };

    class FOpenGLIndexBuffer : public FIndexBuffer {
    public:
        FOpenGLIndexBuffer(const uint32_t* InIndices, unsigned int InCount);
        ~FOpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        unsigned int GetCount() const override { return Count; }
        uint32_t GetRendererID() const { return RendererID; }

    private:
        unsigned int RendererID = 0;
        unsigned int Count = 0;
        size_t AllocatedBytes = 0;
    };

} // namespace Leon
