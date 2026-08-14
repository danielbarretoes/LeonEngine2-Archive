#pragma once

#include "core/Base.hpp"
#include <string>

namespace Leon {

    class FShader {
    public:
        virtual ~FShader() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetInt(const std::string& InName, int InValue) = 0;
        virtual void SetFloat(const std::string& InName, float InValue) = 0;
        virtual void SetFloat3(const std::string& InName, float InX, float InY, float InZ) = 0;
        virtual void SetFloat4(const std::string& InName, float InX, float InY, float InZ, float InW) = 0;
        virtual void SetMat4(const std::string& InName, const float* InMatrix) = 0;

        virtual const std::string& GetName() const = 0;

        static TRef<FShader> Create(const std::string& InFilePath);
        static TRef<FShader> Create(const std::string& InName, const std::string& InVertexSrc,
                                    const std::string& InFragmentSrc);
    };

    using Shader = FShader;

} // namespace Leon
