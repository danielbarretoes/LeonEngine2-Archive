#include "opengl/OpenGLRenderDriver.hpp"
#include "opengl/OpenGLBuffer.hpp"
#include "opengl/OpenGLContext.hpp"
#include "opengl/OpenGLRenderAPI.hpp"
#include "opengl/OpenGLShader.hpp"
#include "opengl/OpenGLTexture2D.hpp"
#include "opengl/OpenGLVertexArray.hpp"

#include <GLFW/glfw3.h>

namespace Leon {

    TScope<IGraphicsContext> FOpenGLRenderDriver::CreateGraphicsContext(void* InWindowHandle) {
        return MakeScope<FOpenGLContext>(static_cast<GLFWwindow*>(InWindowHandle));
    }

    TScope<IRenderAPI> FOpenGLRenderDriver::CreateRenderAPI() {
        return MakeScope<FOpenGLRenderAPI>();
    }

    TRef<FVertexBuffer> FOpenGLRenderDriver::CreateVertexBuffer(unsigned int InSize) {
        return MakeRef<FOpenGLVertexBuffer>(InSize);
    }

    TRef<FVertexBuffer> FOpenGLRenderDriver::CreateVertexBuffer(const float* InVertices, unsigned int InSize) {
        return MakeRef<FOpenGLVertexBuffer>(InVertices, InSize);
    }

    TRef<FIndexBuffer> FOpenGLRenderDriver::CreateIndexBuffer(const uint32_t* InIndices, unsigned int InCount) {
        return MakeRef<FOpenGLIndexBuffer>(InIndices, InCount);
    }

    TRef<FVertexArray> FOpenGLRenderDriver::CreateVertexArray() {
        return MakeRef<FOpenGLVertexArray>();
    }

    TRef<FShader> FOpenGLRenderDriver::CreateShader(const std::string& InFilePath) {
        return MakeRef<FOpenGLShader>(InFilePath);
    }

    TRef<FShader> FOpenGLRenderDriver::CreateShader(const std::string& InName, const std::string& InVertexSrc,
                                                    const std::string& InFragmentSrc) {
        return MakeRef<FOpenGLShader>(InName, InVertexSrc, InFragmentSrc);
    }

    TRef<FTexture2D> FOpenGLRenderDriver::CreateTexture2D(uint32_t InWidth, uint32_t InHeight) {
        return MakeRef<FOpenGLTexture2D>(InWidth, InHeight);
    }

    TRef<FTexture2D> FOpenGLRenderDriver::CreateTexture2D(const std::string& InPath) {
        return MakeRef<FOpenGLTexture2D>(InPath);
    }

    void FOpenGLRenderDriver::Register() {
        FRenderDriverRegistry::RegisterDriver(ERenderAPI::OpenGL, MakeScope<FOpenGLRenderDriver>());
    }

    // Auto-register OpenGL RenderDriver upon library load
    struct FOpenGLDriverRegistration {
        FOpenGLDriverRegistration() { FOpenGLRenderDriver::Register(); }
    };

    static FOpenGLDriverRegistration s_OpenGLDriverRegistration;

} // namespace Leon
