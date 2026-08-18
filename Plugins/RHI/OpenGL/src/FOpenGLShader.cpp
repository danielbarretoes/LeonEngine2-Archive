#include "FOpenGLShader.hpp"
#include "Core/FLog.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
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
        std::filesystem::path path = InFilePath;
        std::string source = ReadFile(InFilePath);
        source = ResolveIncludes(source, path.parent_path().string());
        auto shaderSources = PreProcess(source);
        Compile(shaderSources);

        // Extract name from filepath (e.g. "Assets/Shaders/DefaultLit.glsl" -> "DefaultLit")
        Name = path.stem().string();
    }

    FOpenGLShader::FOpenGLShader(const std::string& InName, const std::string& InVertexSrc,
                                 const std::string& InFragmentSrc)
        : Name(InName) {
        std::unordered_map<GLenum, std::string> sources;
        sources[GL_VERTEX_SHADER] = InVertexSrc;
        sources[GL_FRAGMENT_SHADER] = InFragmentSrc;
        Compile(sources);
    }

    FOpenGLShader::~FOpenGLShader() {
        if (RendererID) {
            glDeleteProgram(RendererID);
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

    std::string FOpenGLShader::ResolveIncludes(const std::string& InSource, const std::string& InBaseDirectory,
                                               int InDepth) {
        constexpr int MaxIncludeDepth = 32;
        if (InDepth > MaxIncludeDepth) {
            LE_CORE_ERROR("Shader include depth exceeded ({0})", MaxIncludeDepth);
            return InSource;
        }

        std::ostringstream out;
        std::istringstream in(InSource);
        std::string line;
        while (std::getline(in, line)) {
            // Strip trailing CR from CRLF files
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const std::string trimmed = [&]() {
                size_t first = line.find_first_not_of(" \t");
                return first == std::string::npos ? std::string() : line.substr(first);
            }();

            if (trimmed.rfind("#include", 0) == 0) {
                size_t quoteOpen = trimmed.find('"');
                size_t quoteClose = (quoteOpen == std::string::npos) ? std::string::npos
                                                                    : trimmed.find('"', quoteOpen + 1);
                if (quoteOpen == std::string::npos || quoteClose == std::string::npos) {
                    LE_CORE_ERROR("Malformed #include in shader (expected #include \"file\"): {0}", line);
                    out << line << '\n';
                    continue;
                }

                const std::string includeName = trimmed.substr(quoteOpen + 1, quoteClose - quoteOpen - 1);
                const std::filesystem::path includePath =
                    std::filesystem::path(InBaseDirectory) / includeName;
                const std::string includeAbs = includePath.lexically_normal().string();
                const std::string included = ReadFile(includeAbs);
                if (included.empty()) {
                    LE_CORE_ERROR("Failed to resolve shader include '{0}'", includeAbs);
                    continue;
                }

                const std::string nestedDir = includePath.parent_path().string();
                out << ResolveIncludes(included, nestedDir, InDepth + 1);
                if (!included.empty() && included.back() != '\n')
                    out << '\n';
            } else {
                out << line << '\n';
            }
        }
        return out.str();
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

            LE_CORE_ERROR("Shader link failure in \"{0}\":\n{1}", Name, infoLog.data());
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

        RendererID = program;
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

    namespace {
        GLuint GBoundProgram = 0;
    }

    void FOpenGLShader::Bind() const {
        if (GBoundProgram == RendererID)
            return;
        GBoundProgram = RendererID;
        glUseProgram(RendererID);
    }

    void FOpenGLShader::Unbind() const {
        GBoundProgram = 0;
        glUseProgram(0);
    }

    int FOpenGLShader::GetUniformLocation(const std::string& InName) const {
        if (UniformLocationCache.find(InName) != UniformLocationCache.end())
            return UniformLocationCache[InName];

        int location = glGetUniformLocation(RendererID, InName.c_str());
        if (location == -1)
            LE_CORE_WARN("Uniform '{0}' not found in shader \"{1}\"!", InName, Name);

        UniformLocationCache[InName] = location;
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
