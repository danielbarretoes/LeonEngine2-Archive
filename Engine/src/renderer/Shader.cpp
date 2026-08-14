#include "renderer/Shader.hpp"
#include "core/Log.hpp"
#include "renderer/RenderDriver.hpp"

namespace Leon {

    TRef<FShader> FShader::Create(const std::string& InFilePath) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateShader(InFilePath);
        }
        LE_CORE_ASSERT(false, "No active RenderDriver registered for Shader creation!");
        return nullptr;
    }

    TRef<FShader> FShader::Create(const std::string& InName, const std::string& InVertexSrc,
                                  const std::string& InFragmentSrc) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateShader(InName, InVertexSrc, InFragmentSrc);
        }
        LE_CORE_ASSERT(false, "No active RenderDriver registered for Shader creation!");
        return nullptr;
    }

} // namespace Leon
