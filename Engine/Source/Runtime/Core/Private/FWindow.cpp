#include "Core/FWindow.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FApplicationEvent.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"

#include <GLFW/glfw3.h>

namespace Leon {

    static uint8_t GLFWWindowCount = 0;

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
        Data.Title = InProps.Title;
        Data.Width = InProps.Width;
        Data.Height = InProps.Height;
        Data.bVSync = InProps.bVSync;

        LE_CORE_INFO("Creating window \"{0}\" ({1}, {2})", InProps.Title, InProps.Width, InProps.Height);

        if (GLFWWindowCount == 0) {
            int success = glfwInit();
            if (!success) {
                LE_CORE_ERROR("Could not initialize GLFW!");
                return;
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // OpenGL 4.5 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
#ifndef NDEBUG
        // Enable OpenGL debug context in Debug builds for glDebugMessageCallback validation
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        NativeWindow = glfwCreateWindow((int)InProps.Width, (int)InProps.Height, Data.Title.c_str(), nullptr, nullptr);
        if (!NativeWindow) {
            LE_CORE_ERROR("Failed to create GLFW window!");
            return;
        }
        ++GLFWWindowCount;

        // Initialize graphics context via RHI
        Context = IGraphicsContext::Create(NativeWindow);
        if (Context) {
            Context->Init();
        } else {
            LE_CORE_ERROR("Failed to create IGraphicsContext! No active RenderDriver is registered.");
        }

        glfwSetWindowUserPointer(NativeWindow, &Data);
        SetVSync(InProps.bVSync);

        // GLFW event callbacks
        glfwSetWindowSizeCallback(NativeWindow, [](GLFWwindow* window, int width, int height) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            FWindowResizeEvent event(width, height);
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(NativeWindow, [](GLFWwindow* window) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FWindowCloseEvent event;
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetKeyCallback(NativeWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
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

        glfwSetMouseButtonCallback(NativeWindow, [](GLFWwindow* window, int button, int action, int mods) {
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

        glfwSetScrollCallback(NativeWindow, [](GLFWwindow* window, double xOffset, double yOffset) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FMouseScrolledEvent event((float)xOffset, (float)yOffset);
            if (data.EventCallback)
                data.EventCallback(event);
        });

        glfwSetCursorPosCallback(NativeWindow, [](GLFWwindow* window, double xPos, double yPos) {
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            FMouseMovedEvent event((float)xPos, (float)yPos);
            if (data.EventCallback)
                data.EventCallback(event);
        });
    }

    void FWindow::Shutdown() {
        if (NativeWindow) {
            glfwDestroyWindow(NativeWindow);
            NativeWindow = nullptr;
            --GLFWWindowCount;

            if (GLFWWindowCount == 0) {
                glfwTerminate();
            }
        }
    }

    void FWindow::OnUpdate() {
        glfwPollEvents();
        if (Context) {
            Context->SwapBuffers();
        }
    }

    void FWindow::SetVSync(bool bInEnabled) {
        if (bInEnabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        Data.bVSync = bInEnabled;
    }

    void FWindow::SetFullscreen(bool bInEnabled) {
        if (!NativeWindow)
            return;

        if (bInEnabled == Data.bFullscreen)
            return;

        if (bInEnabled) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (!monitor)
                return;
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode)
                return;
            glfwSetWindowMonitor(NativeWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            Data.Width = static_cast<unsigned int>(mode->width);
            Data.Height = static_cast<unsigned int>(mode->height);
            Data.bFullscreen = true;
        } else {
            glfwSetWindowMonitor(NativeWindow, nullptr, 100, 100, static_cast<int>(Data.Width),
                                 static_cast<int>(Data.Height), 0);
            Data.bFullscreen = false;
        }

        SetVSync(Data.bVSync);
    }

    void FWindow::SetCursorVisible(bool bVisible) {
        if (!NativeWindow)
            return;
        Data.bCursorVisible = bVisible;
        glfwSetInputMode(NativeWindow, GLFW_CURSOR, bVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

} // namespace Leon
