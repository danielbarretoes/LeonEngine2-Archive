#include "Core/FWindow.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FApplicationEvent.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"

#include <algorithm>
#include <GLFW/glfw3.h>

namespace Leon {

    static uint8_t GLFWWindowCount = 0;

    static void GLFWErrorCallback(int InError, const char* InDescription) {
        LE_CORE_ERROR("GLFW Error ({0}): {1}", InError, InDescription);
    }

    void FWindowDisplayPolicy::ConstrainClientSize(unsigned int& InOutWidth, unsigned int& InOutHeight) {
        if (InOutWidth == 0)
            InOutWidth = DefaultWidth;
        if (InOutHeight == 0)
            InOutHeight = DefaultHeight;

        InOutWidth = std::min(InOutWidth, MaxWidth);
        InOutHeight = std::min(InOutHeight, MaxHeight);

        const unsigned int heightFromWidth =
            InOutWidth * static_cast<unsigned int>(AspectDenominator) / static_cast<unsigned int>(AspectNumerator);
        if (heightFromWidth <= InOutHeight) {
            InOutHeight = heightFromWidth;
        } else {
            InOutWidth =
                InOutHeight * static_cast<unsigned int>(AspectNumerator) / static_cast<unsigned int>(AspectDenominator);
        }

        if (InOutWidth < DefaultWidth || InOutHeight < DefaultHeight) {
            InOutWidth = DefaultWidth;
            InOutHeight = DefaultHeight;
        }
    }

    static void SnapGlfwWindowToLargestHdClient(GLFWwindow* InWindow) {
        unsigned int width = FWindowDisplayPolicy::MaxWidth;
        unsigned int height = FWindowDisplayPolicy::MaxHeight;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
                width = std::min(width, static_cast<unsigned int>(std::max(mode->width, 1)));
                height = std::min(height, static_cast<unsigned int>(std::max(mode->height, 1)));
            }
        }

        FWindowDisplayPolicy::ConstrainClientSize(width, height);
        glfwRestoreWindow(InWindow);
        glfwSetWindowSize(InWindow, static_cast<int>(width), static_cast<int>(height));
    }

    static void PickHdFullscreenMode(GLFWmonitor* InMonitor, int& OutWidth, int& OutHeight, int& OutRefresh) {
        OutWidth = static_cast<int>(FWindowDisplayPolicy::MaxWidth);
        OutHeight = static_cast<int>(FWindowDisplayPolicy::MaxHeight);
        OutRefresh = GLFW_DONT_CARE;

        if (!InMonitor)
            return;

        const GLFWvidmode* current = glfwGetVideoMode(InMonitor);
        if (current)
            OutRefresh = current->refreshRate;

        int modeCount = 0;
        const GLFWvidmode* modes = glfwGetVideoModes(InMonitor, &modeCount);
        if (!modes || modeCount <= 0)
            return;

        int capW = OutWidth;
        int capH = OutHeight;
        if (current) {
            capW = std::min(capW, current->width);
            capH = std::min(capH, current->height);
        }

        const GLFWvidmode* best = nullptr;
        for (int i = 0; i < modeCount; ++i) {
            const GLFWvidmode& mode = modes[i];
            if (mode.width <= 0 || mode.height <= 0)
                continue;
            if (mode.width > capW || mode.height > capH)
                continue;
            if (mode.width * FWindowDisplayPolicy::AspectDenominator !=
                mode.height * FWindowDisplayPolicy::AspectNumerator)
                continue;
            if (!best || mode.width > best->width ||
                (mode.width == best->width && mode.refreshRate > best->refreshRate))
                best = &mode;
        }

        if (best) {
            OutWidth = best->width;
            OutHeight = best->height;
            OutRefresh = best->refreshRate;
        }
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

    void FWindow::ApplyHdClientConstraints() {
        if (!NativeWindow)
            return;
        glfwSetWindowAspectRatio(NativeWindow, FWindowDisplayPolicy::AspectNumerator,
                                 FWindowDisplayPolicy::AspectDenominator);
        glfwSetWindowSizeLimits(NativeWindow, static_cast<int>(FWindowDisplayPolicy::DefaultWidth),
                                static_cast<int>(FWindowDisplayPolicy::DefaultHeight),
                                static_cast<int>(FWindowDisplayPolicy::MaxWidth),
                                static_cast<int>(FWindowDisplayPolicy::MaxHeight));
    }

    void FWindow::Init(const FWindowProps& InProps) {
        Data.Title = InProps.Title;
        Data.Width = InProps.Width;
        Data.Height = InProps.Height;
        Data.bVSync = InProps.bVSync;

        if (InProps.bConstrainAspect) {
            FWindowDisplayPolicy::ConstrainClientSize(Data.Width, Data.Height);
        }

        WindowedWidth = Data.Width;
        WindowedHeight = Data.Height;

        LE_CORE_INFO("Creating window \"{0}\" ({1}, {2})", InProps.Title, Data.Width, Data.Height);

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
        glfwWindowHint(GLFW_RESIZABLE, InProps.bResizable ? GLFW_TRUE : GLFW_FALSE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
#ifndef NDEBUG
        // Enable OpenGL debug context in Debug builds for glDebugMessageCallback validation
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        NativeWindow = glfwCreateWindow((int)Data.Width, (int)Data.Height, Data.Title.c_str(), nullptr, nullptr);
        if (!NativeWindow) {
            LE_CORE_ERROR("Failed to create GLFW window!");
            return;
        }
        ++GLFWWindowCount;

        if (InProps.bConstrainAspect) {
            ApplyHdClientConstraints();
        } else {
            glfwSetWindowSizeLimits(NativeWindow, 400, 300, GLFW_DONT_CARE, GLFW_DONT_CARE);
            glfwSetWindowAspectRatio(NativeWindow, GLFW_DONT_CARE, GLFW_DONT_CARE);
        }
        glfwGetWindowPos(NativeWindow, &WindowedPosX, &WindowedPosY);

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

        glfwSetWindowMaximizeCallback(NativeWindow, [](GLFWwindow* window, int maximized) {
            if (maximized)
                SnapGlfwWindowToLargestHdClient(window);
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
            glfwGetWindowPos(NativeWindow, &WindowedPosX, &WindowedPosY);
            WindowedWidth = Data.Width;
            WindowedHeight = Data.Height;
            FWindowDisplayPolicy::ConstrainClientSize(WindowedWidth, WindowedHeight);

            int width = 0;
            int height = 0;
            int refresh = GLFW_DONT_CARE;
            PickHdFullscreenMode(monitor, width, height, refresh);
            glfwSetWindowMonitor(NativeWindow, monitor, 0, 0, width, height, refresh);
            Data.Width = static_cast<unsigned int>(width);
            Data.Height = static_cast<unsigned int>(height);
            Data.bFullscreen = true;
        } else {
            glfwSetWindowMonitor(NativeWindow, nullptr, WindowedPosX, WindowedPosY, static_cast<int>(WindowedWidth),
                                 static_cast<int>(WindowedHeight), 0);
            Data.Width = WindowedWidth;
            Data.Height = WindowedHeight;
            Data.bFullscreen = false;
            ApplyHdClientConstraints();
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
