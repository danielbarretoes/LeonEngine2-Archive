#include "engine/renderer/RenderAPI.hpp"
#include "engine/renderer/RenderDriver.hpp"

namespace Leon {

    ERenderAPI IRenderAPI::s_API = ERenderAPI::OpenGL;

    TScope<IRenderAPI> IRenderAPI::Create() {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateRenderAPI();
        }
        return nullptr;
    }

} // namespace Leon
