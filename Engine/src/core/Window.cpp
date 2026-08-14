#include "core/Window.hpp"
#include "core/Log.hpp"
#include "core/events/ApplicationEvent.hpp"
#include "core/events/KeyEvent.hpp"
#include "core/events/MouseEvent.hpp"

#include <GLFW/glfw3.h>

namespace Leon {

    static uint8_t s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int InError, const char* InDescription) {
        LE_CORE_ERROR("GLFW Error ({0}): {1}", InError, InDescription);
    }

    TScope<FWindow> FWindow::Create(const FWindowProps& InProps) {
        return MakeScope<FWindow>(InProps);
    }

    FWindow::FWindow(const FWindowProps& InProps) {
        Init(InProps);
    }

    FWindow::~FWindow() {
        Shutdown();
    }

    void FWindow::Init(const FWindowProps& InProps) {
        m_Data.Title = InProps.Title;
        m_Data.Width = InProps.Width;
        m_Data.Height = InProps.Height;
        m_Data.bVSync = InProps.bVSync;

        LE_CORE_INFO("Creating window \"{0}\" ({1}, {2})", InProps.Title, InProps.Width, InProps.Height);

        if (s_GLFWWindowCount == 0) {
            int success = glfwInit();
            if (!success) {
                LE_CORE_ERROR("Could not initialize GLFW!");
                return;
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // Standard modern context profile hints
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        m_Window = glfwCreateWindow((int)InProps.Width, (int)InProps.Height, m_Data.Title.c_str(), nullptr, nullptr);
        if (!m_Window) {
            LE_CORE_ERROR("Failed to create GLFW window!");
            return;
        }
        ++s_GLFWWindowCount;

        // Initialize graphics context via RHI
        m_Context = IGraphicsContext::Create(m_Window);
        if (m_Context) {
            m_Context->Init();
        } else {
            LE_CORE_ERROR("Failed to create GraphicsContext! No active RenderDriver is registered.");
        }

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(InProps.bVSync);

        // GLFW event callbacks
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            FWindowResizeEvent event(width, height);
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FWindowCloseEvent event;
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);

            switch (action) {
            case GLFW_PRESS: {
                FKeyPressedEvent event(key, false);
                if (data.EventCallback)
                    data.EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                FKeyReleasedEvent event(key);
                if (data.EventCallback)
                    data.EventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                FKeyPressedEvent event(key, true);
                if (data.EventCallback)
                    data.EventCallback(event);
                break;
            }
            }
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);

            switch (action) {
            case GLFW_PRESS: {
                FMouseButtonPressedEvent event(button);
                if (data.EventCallback)
                    data.EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                FMouseButtonReleasedEvent event(button);
                if (data.EventCallback)
                    data.EventCallback(event);
                break;
            }
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FMouseScrolledEvent event((float)xOffset, (float)yOffset);
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FMouseMovedEvent event((float)xPos, (float)yPos);
            if (data.EventCallback)
                data.EventCallback(event);
        });
    }

    void FWindow::Shutdown() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            --s_GLFWWindowCount;

            if (s_GLFWWindowCount == 0) {
                glfwTerminate();
            }
        }
    }

    void FWindow::OnUpdate() {
        glfwPollEvents();
        if (m_Context) {
            m_Context->SwapBuffers();
        }
    }

    void FWindow::SetVSync(bool bInEnabled) {
        if (bInEnabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_Data.bVSync = bInEnabled;
    }

} // namespace Leon
