#include "engine/renderer/VertexArray.hpp"
#include "engine/renderer/RenderDriver.hpp"

namespace Leon {

    TRef<FVertexArray> FVertexArray::Create() {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateVertexArray();
        }
        return nullptr;
    }

} // namespace Leon
