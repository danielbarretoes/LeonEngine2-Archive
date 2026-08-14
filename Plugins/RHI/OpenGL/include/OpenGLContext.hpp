#pragma once

#include "renderer/GraphicsContext.hpp"

struct GLFWwindow;

namespace Leon {

    class FOpenGLContext : public IGraphicsContext {
    public:
        FOpenGLContext(GLFWwindow* InWindowHandle);

        void Init() override;
        void SwapBuffers() override;

    private:
        GLFWwindow* m_WindowHandle;
    };

    using OpenGLContext = FOpenGLContext;

} // namespace Leon
