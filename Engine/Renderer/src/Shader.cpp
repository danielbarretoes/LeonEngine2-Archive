#include "engine/renderer/Shader.hpp"
#include "engine/renderer/RenderDriver.hpp"

namespace Leon {

    TRef<FShader> FShader::Create(const std::string& InName, const std::string& InVertexSrc,
                                  const std::string& InFragmentSrc) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateShader(InName, InVertexSrc, InFragmentSrc);
        }
        return nullptr;
    }

    TRef<FShader> FShader::Create(const std::string& InVertexSrc, const std::string& InFragmentSrc) {
        return Create("UnnamedShader", InVertexSrc, InFragmentSrc);
    }

} // namespace Leon
