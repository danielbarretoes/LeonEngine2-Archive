#pragma once

#include "renderer/VertexArray.hpp"
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

        const std::vector<TRef<FVertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
        const TRef<FIndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

    private:
        unsigned int m_RendererID = 0;
        uint32_t m_VertexBufferIndex = 0;
        std::vector<TRef<FVertexBuffer>> m_VertexBuffers;
        TRef<FIndexBuffer> m_IndexBuffer;
    };

    using OpenGLVertexArray = FOpenGLVertexArray;

} // namespace Leon
