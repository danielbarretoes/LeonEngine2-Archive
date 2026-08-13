#include "plugin_opengl/OpenGLContext.hpp"
#include "engine/core/Log.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Leon {

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
    }

    void FOpenGLContext::SwapBuffers() {
        glfwSwapBuffers(m_WindowHandle);
    }

} // namespace Leon
