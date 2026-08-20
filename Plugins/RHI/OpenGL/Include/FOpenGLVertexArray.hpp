#pragma once

#include "RHI/FVertexArray.hpp"
#include <vector>

namespace Leon {

    class FOpenGLVertexArray : public FVertexArray {
    public:
        FOpenGLVertexArray();
        ~FOpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const TRef<FVertexBuffer>& InVertexBuffer) override;
        void SetIndexBuffer(const TRef<FIndexBuffer>& InIndexBuffer) override;

        const std::vector<TRef<FVertexBuffer>>& GetVertexBuffers() const override { return VertexBuffers; }
        const TRef<FIndexBuffer>& GetIndexBuffer() const override { return IndexBuffer; }

    private:
        unsigned int RendererID = 0;
        uint32_t VertexBufferIndex = 0;
        std::vector<TRef<FVertexBuffer>> VertexBuffers;
        TRef<FIndexBuffer> IndexBuffer;
    };

} // namespace Leon
