#include "OpenGLRenderDriver.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLContext.hpp"
#include "OpenGLRenderAPI.hpp"
#include "OpenGLFramebuffer.hpp"
#include "OpenGLShader.hpp"
#include "OpenGLTexture2D.hpp"
#include "OpenGLVertexArray.hpp"

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

    TRef<FFramebuffer> FOpenGLRenderDriver::CreateFramebuffer(const FFramebufferSpecification& InSpec) {
        return MakeRef<FOpenGLFramebuffer>(InSpec);
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
