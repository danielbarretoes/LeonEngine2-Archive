#pragma once

#include "RHI/IGraphicsContext.hpp"

struct GLFWwindow;

namespace Leon {

    class FOpenGLContext : public IGraphicsContext {
    public:
        FOpenGLContext(GLFWwindow* InWindowHandle);

        void Init() override;
        void SwapBuffers() override;

    private:
        GLFWwindow* WindowHandle;
    };

} // namespace Leon
