#pragma once

#include "Core/Base.hpp"
#include "RHI/FBuffer.hpp"
#include <memory>
#include <vector>

namespace Leon {

    class FVertexArray {
    public:
        virtual ~FVertexArray() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void AddVertexBuffer(const TRef<FVertexBuffer>& InVertexBuffer) = 0;
        virtual void SetIndexBuffer(const TRef<FIndexBuffer>& InIndexBuffer) = 0;

        virtual const std::vector<TRef<FVertexBuffer>>& GetVertexBuffers() const = 0;
        virtual const TRef<FIndexBuffer>& GetIndexBuffer() const = 0;

        static TRef<FVertexArray> Create();
    };

} // namespace Leon
