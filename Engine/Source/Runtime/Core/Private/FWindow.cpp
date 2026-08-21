#include "Core/FWindow.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FApplicationEvent.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"

#include <algorithm>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif
#include <stb_image.h>

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
        Data.bHdClientPolicy = InProps.bConstrainAspect;

        if (Data.bHdClientPolicy) {
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
        glfwWindowHint(GLFW_MAXIMIZED, InProps.bMaximized ? GLFW_TRUE : GLFW_FALSE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
#ifndef NDEBUG
        // Enable OpenGL debug context in Debug builds for glDebugMessageCallback validation
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        NativeWindow = glfwCreateWindow((int)Data.Width, (int)Data.Height, Data.Title.c_str(), nullptr, nullptr);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
        if (!NativeWindow) {
            LE_CORE_ERROR("Failed to create GLFW window!");
            return;
        }
        ++GLFWWindowCount;

        if (Data.bHdClientPolicy) {
            ApplyHdClientConstraints();
        } else {
            glfwSetWindowSizeLimits(NativeWindow, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
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
        ApplyEmbeddedWin32Icon();

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
            FWindowData& data = *(FWindowData*)glfwGetWindowUserPointer(window);
            if (maximized && data.bHdClientPolicy)
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
            if (Data.bHdClientPolicy)
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

    void FWindow::ApplyEmbeddedWin32Icon() {
#ifdef _WIN32
        if (!NativeWindow)
            return;
        HWND hwnd = glfwGetWin32Window(NativeWindow);
        if (!hwnd)
            return;

        HINSTANCE inst = GetModuleHandleW(nullptr);
        const int smallW = GetSystemMetrics(SM_CXSMICON);
        const int smallH = GetSystemMetrics(SM_CYSMICON);
        HICON smallIcon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, smallW, smallH,
                                                        LR_DEFAULTCOLOR | LR_SHARED));
        HICON bigIcon = static_cast<HICON>(
            LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED));
        if (!bigIcon)
            bigIcon = static_cast<HICON>(
                LoadImageW(inst, L"GLFW_ICON", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
        if (!smallIcon && bigIcon)
            smallIcon = bigIcon;

        if (bigIcon) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
            SetClassLongPtrW(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(bigIcon));
        }
        if (smallIcon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
            SetClassLongPtrW(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(smallIcon));
        }
#endif
    }

    void FWindow::SetIconFromFile(const std::string& InPath) {
        if (!NativeWindow || InPath.empty())
            return;

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_set_flip_vertically_on_load(0);
        stbi_uc* pixels = stbi_load(InPath.c_str(), &width, &height, &channels, 4);
        if (!pixels || width <= 0 || height <= 0) {
            if (pixels)
                stbi_image_free(pixels);
            return;
        }

        const int sizes[] = {16, 32, 48, 256};
        std::vector<std::vector<unsigned char>> buffers;
        std::vector<GLFWimage> images;
        buffers.reserve(4);
        images.reserve(4);

        auto downscale = [&](int dw, int dh) {
            std::vector<unsigned char> dst(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4u);
            for (int y = 0; y < dh; ++y) {
                const int y0 = y * height / dh;
                const int y1 = std::max(y0 + 1, (y + 1) * height / dh);
                for (int x = 0; x < dw; ++x) {
                    const int x0 = x * width / dw;
                    const int x1 = std::max(x0 + 1, (x + 1) * width / dw);
                    unsigned int r = 0, g = 0, b = 0, a = 0, n = 0;
                    for (int sy = y0; sy < y1; ++sy) {
                        for (int sx = x0; sx < x1; ++sx) {
                            const stbi_uc* p = pixels + (sy * width + sx) * 4;
                            r += p[0];
                            g += p[1];
                            b += p[2];
                            a += p[3];
                            ++n;
                        }
                    }
                    unsigned char* d = dst.data() + (y * dw + x) * 4;
                    d[0] = static_cast<unsigned char>(r / n);
                    d[1] = static_cast<unsigned char>(g / n);
                    d[2] = static_cast<unsigned char>(b / n);
                    d[3] = static_cast<unsigned char>(a / n);
                }
            }
            return dst;
        };

        for (int size : sizes) {
            if (width == size && height == size) {
                buffers.emplace_back(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
            } else {
                buffers.push_back(downscale(size, size));
            }
            GLFWimage img{};
            img.width = size;
            img.height = size;
            img.pixels = buffers.back().data();
            images.push_back(img);
        }

        glfwSetWindowIcon(NativeWindow, static_cast<int>(images.size()), images.data());
        stbi_image_free(pixels);
    }

} // namespace Leon
