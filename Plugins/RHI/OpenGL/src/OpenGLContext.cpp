#include "OpenGLContext.hpp"
#include "core/Log.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Leon {

#ifndef NDEBUG
    static void APIENTRY OpenGLDebugCallback(GLenum source, GLenum type, GLuint id,
                                              GLenum severity, GLsizei /*length*/,
                                              const GLchar* message, const void* /*userParam*/) {
        // Filter out non-significant notification messages and driver texture base-level warnings
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION || id == 131204 || id == 131218)
            return;

        const char* sourceStr = "Unknown";
        switch (source) {
            case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "ShaderCompiler"; break;
            case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
            case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "ThirdParty"; break;
            default: break;
        }

        const char* typeStr = "Unknown";
        switch (type) {
            case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "UndefinedBehavior"; break;
            case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
            case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
            default: break;
        }

        if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR) {
            LE_CORE_ERROR("[GL Debug] [{0}] [{1}] (id={2}): {3}", sourceStr, typeStr, id, message);
        } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
            LE_CORE_WARN("[GL Debug] [{0}] [{1}] (id={2}): {3}", sourceStr, typeStr, id, message);
        } else {
            LE_CORE_TRACE("[GL Debug] [{0}] [{1}] (id={2}): {3}", sourceStr, typeStr, id, message);
        }
    }
#endif

    FOpenGLContext::FOpenGLContext(GLFWwindow* InWindowHandle) : m_WindowHandle(InWindowHandle) {}

    void FOpenGLContext::Init() {
        glfwMakeContextCurrent(m_WindowHandle);
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if (!status) {
            LE_CORE_ERROR("Failed to initialize GLAD!");
            return;
        }

        LE_CORE_INFO("OpenGL Info:");
        LE_CORE_INFO("  Vendor:   {0}", (const char*)glGetString(GL_VENDOR));
        LE_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        LE_CORE_INFO("  Version:  {0}", (const char*)glGetString(GL_VERSION));

#ifndef NDEBUG
        // Register OpenGL 4.3+ KHR_debug message callback for silent-error detection
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OpenGLDebugCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            LE_CORE_INFO("  [GL] Debug context active — glDebugMessageCallback registered.");
        } else {
            LE_CORE_WARN("  [GL] Debug context requested but not available from driver.");
        }
#endif
    }

    void FOpenGLContext::SwapBuffers() {
        glfwSwapBuffers(m_WindowHandle);
    }

} // namespace Leon
