#pragma once

#include "engine/renderer/Shader.hpp"
#include <string>
#include <unordered_map>

namespace Leon {

    class FOpenGLShader : public FShader {
    public:
        FOpenGLShader(const std::string& InName, const std::string& InVertexSrc, const std::string& InFragmentSrc);
        FOpenGLShader(const std::string& InVertexSrc, const std::string& InFragmentSrc);
        ~FOpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void SetInt(const std::string& InName, int InValue) override;
        void SetFloat(const std::string& InName, float InValue) override;
        void SetFloat3(const std::string& InName, float InX, float InY, float InZ) override;
        void SetFloat4(const std::string& InName, float InX, float InY, float InZ, float InW) override;
        void SetMat4(const std::string& InName, const float* InMatrix) override;

        const std::string& GetName() const override { return m_Name; }

    private:
        unsigned int CompileShader(unsigned int InType, const std::string& InSource);
        int GetUniformLocation(const std::string& InName) const;

    private:
        unsigned int m_RendererID = 0;
        std::string m_Name;
        mutable std::unordered_map<std::string, int> m_UniformLocationCache;
    };

    using OpenGLShader = FOpenGLShader;

} // namespace Leon
