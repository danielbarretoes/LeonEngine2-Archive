#include "RHI/FShader.hpp"
#include "Core/FLog.hpp"
#include "RHI/IRenderDriver.hpp"

namespace Leon {

    TRef<FShader> FShader::Create(const std::string& InFilePath) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateShader(InFilePath);
        }
        return nullptr;
    }

    TRef<FShader> FShader::Create(const std::string& InName, const std::string& InVertexSrc,
                                  const std::string& InFragmentSrc) {
        if (auto driver = FRenderDriverRegistry::GetActiveDriver()) {
            return driver->CreateShader(InName, InVertexSrc, InFragmentSrc);
        }
        return nullptr;
    }

} // namespace Leon
