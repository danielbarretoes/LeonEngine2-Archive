#pragma once

#include "RHI/FShader.hpp"
#include <glad/glad.h>
#include <string>
#include <unordered_map>

namespace Leon {

    class FOpenGLShader : public FShader {
    public:
        FOpenGLShader(const std::string& InFilePath);
        FOpenGLShader(const std::string& InName, const std::string& InVertexSrc, const std::string& InFragmentSrc);
        ~FOpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;
        static void InvalidateBoundCache();

        void SetInt(const std::string& InName, int InValue) override;
        void SetFloat(const std::string& InName, float InValue) override;
        void SetFloat2(const std::string& InName, float InX, float InY) override;
        void SetFloat3(const std::string& InName, float InX, float InY, float InZ) override;

        void SetFloat4(const std::string& InName, float InX, float InY, float InZ, float InW) override;
        void SetMat3(const std::string& InName, const float* InMatrix) override;
        void SetMat4(const std::string& InName, const float* InMatrix) override;

        const std::string& GetName() const override { return Name; }

    private:
        std::string ReadFile(const std::string& InFilePath);
        /** Resolve `#include "file.glsl"` relative to InBaseDirectory (recursive, cycle-safe). */
        std::string ResolveIncludes(const std::string& InSource, const std::string& InBaseDirectory,
                                    int InDepth = 0);
        std::unordered_map<GLenum, std::string> PreProcess(const std::string& InSource);
        void Compile(const std::unordered_map<GLenum, std::string>& InShaderSources);
        unsigned int CompileShader(unsigned int InType, const std::string& InSource);
        int GetUniformLocation(const std::string& InName) const;

    private:
        unsigned int RendererID = 0;
        std::string Name;
        mutable std::unordered_map<std::string, int> UniformLocationCache;
    };

} // namespace Leon
