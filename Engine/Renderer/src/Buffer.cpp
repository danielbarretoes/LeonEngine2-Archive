#include "engine/renderer/Buffer.hpp"
#include "engine/renderer/RenderDriver.hpp"

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

} // namespace Leon
