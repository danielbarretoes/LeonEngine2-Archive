#include "RHI/FVertexArray.hpp"
#include "RHI/IRenderDriver.hpp"

namespace Leon {

    TRef<FVertexArray> FVertexArray::Create() {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateVertexArray();
        }
        return nullptr;
    }

} // namespace Leon
