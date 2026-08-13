#include "plugin_opengl/OpenGLRenderDriver.hpp"
#include "plugin_opengl/OpenGLBuffer.hpp"
#include "plugin_opengl/OpenGLContext.hpp"
#include "plugin_opengl/OpenGLRenderAPI.hpp"
#include "plugin_opengl/OpenGLShader.hpp"
#include "plugin_opengl/OpenGLVertexArray.hpp"

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

    TRef<FShader> FOpenGLRenderDriver::CreateShader(const std::string& InName, const std::string& InVertexSrc,
                                                    const std::string& InFragmentSrc) {
        return MakeRef<FOpenGLShader>(InName, InVertexSrc, InFragmentSrc);
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
