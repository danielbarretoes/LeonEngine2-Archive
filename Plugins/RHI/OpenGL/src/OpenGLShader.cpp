#include "OpenGLShader.hpp"
#include "core/Log.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace Leon {

    static GLenum ShaderTypeFromString(const std::string& InType) {
        if (InType == "vertex")
            return GL_VERTEX_SHADER;
        if (InType == "fragment" || InType == "pixel")
            return GL_FRAGMENT_SHADER;

        LE_CORE_ASSERT(false, "Unknown shader type!");
        return 0;
    }

    FOpenGLShader::FOpenGLShader(const std::string& InFilePath) {
        std::string source = ReadFile(InFilePath);
        auto shaderSources = PreProcess(source);
        Compile(shaderSources);

        // Extract name from filepath (e.g. "Assets/Shaders/DefaultLit.glsl" -> "DefaultLit")
        std::filesystem::path path = InFilePath;
        m_Name = path.stem().string();
    }

    FOpenGLShader::FOpenGLShader(const std::string& InName, const std::string& InVertexSrc,
                                 const std::string& InFragmentSrc)
        : m_Name(InName) {
        std::unordered_map<GLenum, std::string> sources;
        sources[GL_VERTEX_SHADER] = InVertexSrc;
        sources[GL_FRAGMENT_SHADER] = InFragmentSrc;
        Compile(sources);
    }

    FOpenGLShader::~FOpenGLShader() {
        if (m_RendererID) {
            glDeleteProgram(m_RendererID);
        }
    }

    std::string FOpenGLShader::ReadFile(const std::string& InFilePath) {
        std::string result;
        std::ifstream in(InFilePath, std::ios::in | std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            size_t size = in.tellg();
            if (size != -1) {
                result.resize(size);
                in.seekg(0, std::ios::beg);
                in.read(&result[0], size);
            } else {
                LE_CORE_ERROR("Could not read from file '{0}'", InFilePath);
            }
        } else {
            LE_CORE_ERROR("Could not open file '{0}'", InFilePath);
        }
        return result;
    }

    std::unordered_map<GLenum, std::string> FOpenGLShader::PreProcess(const std::string& InSource) {
        std::unordered_map<GLenum, std::string> shaderSources;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = InSource.find(typeToken, 0);

        while (pos != std::string::npos) {
            size_t eol = InSource.find_first_of("\r\n", pos);
            LE_CORE_ASSERT(eol != std::string::npos, "Syntax error in shader preprocessor line");
            size_t begin = pos + typeTokenLength + 1;
            std::string type = InSource.substr(begin, eol - begin);

            // Trim any trailing whitespace
            while (!type.empty() && (type.back() == ' ' || type.back() == '\t' || type.back() == '\r')) {
                type.pop_back();
            }

            size_t nextLinePos = InSource.find_first_not_of("\r\n", eol);
            pos = InSource.find(typeToken, nextLinePos);

            shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos)
                                                            ? InSource.substr(nextLinePos)
                                                            : InSource.substr(nextLinePos, pos - nextLinePos);
        }

        return shaderSources;
    }

    void FOpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& InShaderSources) {
        GLuint program = glCreateProgram();
        std::vector<GLenum> glShaderIDs;
        glShaderIDs.reserve(InShaderSources.size());

        for (auto& kv : InShaderSources) {
            GLenum type = kv.first;
            const std::string& source = kv.second;

            GLuint shader = CompileShader(type, source);
            if (shader) {
                glAttachShader(program, shader);
                glShaderIDs.push_back(shader);
            }
        }

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE) {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(program);
            for (auto id : glShaderIDs) {
                glDeleteShader(id);
            }

            LE_CORE_ERROR("Shader link failure in \"{0}\":\n{1}", m_Name, infoLog.data());
            return;
        }

        for (auto id : glShaderIDs) {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        GLuint cameraBlockIndex = glGetUniformBlockIndex(program, "CameraData");
        if (cameraBlockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(program, cameraBlockIndex, 0);
        }
        GLuint lightingBlockIndex = glGetUniformBlockIndex(program, "LightingData");
        if (lightingBlockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(program, lightingBlockIndex, 1);
        }

        m_RendererID = program;
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

            LE_CORE_ERROR("Shader compilation failure ({0}):\n{1}",
                          (InType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"), infoLog.data());
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
            LE_CORE_WARN("Uniform '{0}' not found in shader \"{1}\"!", InName, m_Name);

        m_UniformLocationCache[InName] = location;
        return location;
    }

    void FOpenGLShader::SetInt(const std::string& InName, int InValue) {
        glUniform1i(GetUniformLocation(InName), InValue);
    }

    void FOpenGLShader::SetFloat(const std::string& InName, float InValue) {
        glUniform1f(GetUniformLocation(InName), InValue);
    }

    void FOpenGLShader::SetFloat2(const std::string& InName, float InX, float InY) {
        glUniform2f(GetUniformLocation(InName), InX, InY);
    }

    void FOpenGLShader::SetFloat3(const std::string& InName, float InX, float InY, float InZ) {

        glUniform3f(GetUniformLocation(InName), InX, InY, InZ);
    }

    void FOpenGLShader::SetFloat4(const std::string& InName, float InX, float InY, float InZ, float InW) {
        glUniform4f(GetUniformLocation(InName), InX, InY, InZ, InW);
    }

    void FOpenGLShader::SetMat3(const std::string& InName, const float* InMatrix) {
        glUniformMatrix3fv(GetUniformLocation(InName), 1, GL_FALSE, InMatrix);
    }

    void FOpenGLShader::SetMat4(const std::string& InName, const float* InMatrix) {
        glUniformMatrix4fv(GetUniformLocation(InName), 1, GL_FALSE, InMatrix);
    }

} // namespace Leon
