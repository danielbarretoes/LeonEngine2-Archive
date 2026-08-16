#include "RHI/FBuffer.hpp"
#include "RHI/IRenderDriver.hpp"

namespace Leon {

    TRef<FVertexBuffer> FVertexBuffer::Create(unsigned int InSize) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateVertexBuffer(InSize);
        }
        return nullptr;
    }

    TRef<FVertexBuffer> FVertexBuffer::Create(const float* InVertices, unsigned int InSize) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateVertexBuffer(InVertices, InSize);
        }
        return nullptr;
    }

    TRef<FIndexBuffer> FIndexBuffer::Create(const uint32_t* InIndices, unsigned int InCount) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateIndexBuffer(InIndices, InCount);
        }
        return nullptr;
    }

    TRef<FUniformBuffer> FUniformBuffer::Create(unsigned int InSize, unsigned int InBinding) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateUniformBuffer(InSize, InBinding);
        }
        return nullptr;
    }

} // namespace Leon
