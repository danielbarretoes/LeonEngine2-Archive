#include "plugin_opengl/OpenGLShader.hpp"
#include "engine/core/Log.hpp"

#include <glad/glad.h>
#include <vector>

namespace Leon {

    FOpenGLShader::FOpenGLShader(const std::string& InName, const std::string& InVertexSrc,
                                 const std::string& InFragmentSrc)
        : m_Name(InName) {
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, InVertexSrc);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, InFragmentSrc);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vertexShader);
        glAttachShader(m_RendererID, fragmentShader);
        glLinkProgram(m_RendererID);

        GLint isLinked = 0;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(m_RendererID, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(m_RendererID);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            LE_CORE_ERROR("Shader link failure in \"{0}\": {1}", m_Name, infoLog.data());
            return;
        }

        glDetachShader(m_RendererID, vertexShader);
        glDetachShader(m_RendererID, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    FOpenGLShader::FOpenGLShader(const std::string& InVertexSrc, const std::string& InFragmentSrc)
        : FOpenGLShader("UnnamedShader", InVertexSrc, InFragmentSrc) {}

    FOpenGLShader::~FOpenGLShader() {
        glDeleteProgram(m_RendererID);
    }

    unsigned int FOpenGLShader::CompileShader(unsigned int InType, const std::string& InSource) {
        GLuint shader = glCreateShader(InType);
        const GLchar* sourceCStr = InSource.c_str();
        glShaderSource(shader, 1, &sourceCStr, 0);
        glCompileShader(shader);

        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

            glDeleteShader(shader);

            LE_CORE_ERROR("Shader compilation failure ({0}): {1}", (InType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"),
                          infoLog.data());
            return 0;
        }

        return shader;
    }

    void FOpenGLShader::Bind() const {
        glUseProgram(m_RendererID);
    }

    void FOpenGLShader::Unbind() const {
        glUseProgram(0);
    }

    int FOpenGLShader::GetUniformLocation(const std::string& InName) const {
        if (m_UniformLocationCache.find(InName) != m_UniformLocationCache.end())
            return m_UniformLocationCache[InName];

        int location = glGetUniformLocation(m_RendererID, InName.c_str());
        if (location == -1)
            LE_CORE_WARN("Uniform '{0}' not found!", InName);

        m_UniformLocationCache[InName] = location;
        return location;
    }

    void FOpenGLShader::SetInt(const std::string& InName, int InValue) {
        glUniform1i(GetUniformLocation(InName), InValue);
    }

    void FOpenGLShader::SetFloat(const std::string& InName, float InValue) {
        glUniform1f(GetUniformLocation(InName), InValue);
    }

    void FOpenGLShader::SetFloat3(const std::string& InName, float InX, float InY, float InZ) {
        glUniform3f(GetUniformLocation(InName), InX, InY, InZ);
    }

    void FOpenGLShader::SetFloat4(const std::string& InName, float InX, float InY, float InZ, float InW) {
        glUniform4f(GetUniformLocation(InName), InX, InY, InZ, InW);
    }

    void FOpenGLShader::SetMat4(const std::string& InName, const float* InMatrix) {
        glUniformMatrix4fv(GetUniformLocation(InName), 1, GL_FALSE, InMatrix);
    }

} // namespace Leon
