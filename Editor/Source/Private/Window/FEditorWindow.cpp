#include "Editor/Window/FEditorWindow.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FApplicationEvent.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"

#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>

namespace Leon::Editor {

    static uint8_t GLFWEditorWindowCount = 0;

    static void GLFWErrorCallback(int InError, const char* InDescription) {
        LE_CORE_ERROR("GLFW Editor Window Error ({}): {}", InError, InDescription);
    }

    FEditorWindow::FEditorWindow(const FEditorWindowProps& InProps) {
        Init(InProps);
    }

    FEditorWindow::~FEditorWindow() {
        Shutdown();
    }

    void FEditorWindow::Init(const FEditorWindowProps& InProps) {
        BaseTitle = InProps.Title;
        WindowWidth = InProps.Width;
        WindowHeight = InProps.Height;
        bVSync = InProps.bVSync;

        LE_CORE_INFO("FEditorWindow: Creating editor window \"{}\" ({}x{})", BaseTitle, WindowWidth, WindowHeight);

        if (GLFWEditorWindowCount == 0) {
            int success = glfwInit();
            if (!success) {
                LE_CORE_ERROR("FEditorWindow: Could not initialize GLFW!");
                return;
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // OpenGL 4.5 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_MAXIMIZED, InProps.bMaximized ? GLFW_TRUE : GLFW_FALSE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
#ifndef NDEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        NativeWindow = glfwCreateWindow(static_cast<int>(WindowWidth), static_cast<int>(WindowHeight),
                                        BaseTitle.c_str(), nullptr, nullptr);
        if (!NativeWindow) {
            LE_CORE_ERROR("FEditorWindow: Failed to create GLFW editor window!");
            return;
        }
        ++GLFWEditorWindowCount;

        // Unconstrained resizing with safe minimums
        glfwSetWindowSizeLimits(NativeWindow, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwSetWindowAspectRatio(NativeWindow, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwGetWindowPos(NativeWindow, &WindowPosX, &WindowPosY);

        // Initialize graphics context via RHI
        Context = IGraphicsContext::Create(NativeWindow);
        if (Context) {
            Context->Init();
        } else {
            LE_CORE_ERROR("FEditorWindow: Failed to create IGraphicsContext!");
        }

        glfwSetWindowUserPointer(NativeWindow, this);
        SetVSync(InProps.bVSync);

        // GLFW Event Callbacks
        glfwSetWindowSizeCallback(NativeWindow, [](GLFWwindow* window, int width, int height) {
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;
            self->WindowWidth = static_cast<unsigned int>(width);
            self->WindowHeight = static_cast<unsigned int>(height);

            FWindowResizeEvent event(self->WindowWidth, self->WindowHeight);
            if (self->EventCallback)
                self->EventCallback(event);
        });

        glfwSetWindowCloseCallback(NativeWindow, [](GLFWwindow* window) {
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;
            FWindowCloseEvent event;
            if (self->EventCallback)
                self->EventCallback(event);
        });

        glfwSetKeyCallback(NativeWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            (void)scancode;
            (void)mods;
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;

            switch (action) {
            case GLFW_PRESS: {
                FKeyPressedEvent event(key, 0);
                if (self->EventCallback)
                    self->EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                FKeyReleasedEvent event(key);
                if (self->EventCallback)
                    self->EventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                FKeyPressedEvent event(key, 1);
                if (self->EventCallback)
                    self->EventCallback(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetMouseButtonCallback(NativeWindow, [](GLFWwindow* window, int button, int action, int mods) {
            (void)mods;
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;

            switch (action) {
            case GLFW_PRESS: {
                FMouseButtonPressedEvent event(button);
                if (self->EventCallback)
                    self->EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                FMouseButtonReleasedEvent event(button);
                if (self->EventCallback)
                    self->EventCallback(event);
                break;
            }
            default:
                break;
            }
        });

        glfwSetScrollCallback(NativeWindow, [](GLFWwindow* window, double xOffset, double yOffset) {
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;
            FMouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
            if (self->EventCallback)
                self->EventCallback(event);
        });

        glfwSetCursorPosCallback(NativeWindow, [](GLFWwindow* window, double xPos, double yPos) {
            auto* self = static_cast<FEditorWindow*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;
            FMouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
            if (self->EventCallback)
                self->EventCallback(event);
        });
    }

    void FEditorWindow::Shutdown() {
        if (NativeWindow) {
            glfwDestroyWindow(NativeWindow);
            NativeWindow = nullptr;
            --GLFWEditorWindowCount;
            if (GLFWEditorWindowCount == 0) {
                glfwTerminate();
            }
        }
    }

    void FEditorWindow::OnUpdate() {
        glfwPollEvents();
        if (Context) {
            Context->SwapBuffers();
        }
    }

    void FEditorWindow::SetVSync(bool bInEnabled) {
        if (bInEnabled) {
            glfwSwapInterval(1);
        } else {
            glfwSwapInterval(0);
        }
        bVSync = bInEnabled;
    }

    void FEditorWindow::Maximize() {
        if (NativeWindow) {
            glfwMaximizeWindow(NativeWindow);
        }
    }

    void FEditorWindow::Restore() {
        if (NativeWindow) {
            glfwRestoreWindow(NativeWindow);
        }
    }

    bool FEditorWindow::IsMaximized() const {
        if (!NativeWindow)
            return false;
        return glfwGetWindowAttrib(NativeWindow, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    void FEditorWindow::SetTitle(const std::string& InTitle) {
        BaseTitle = InTitle;
        if (NativeWindow) {
            glfwSetWindowTitle(NativeWindow, BaseTitle.c_str());
        }
    }

    void FEditorWindow::UpdateEditorTitle(const std::string& InProjectName, const std::string& InMapName) {
        std::string fullTitle = "Leon Engine Editor";
        if (!InProjectName.empty()) {
            fullTitle += " - [" + InProjectName + "]";
        }
        if (!InMapName.empty()) {
            fullTitle += " - " + InMapName;
        }
        SetTitle(fullTitle);
    }

    bool FEditorWindow::ShouldClose() const {
        return NativeWindow ? (glfwWindowShouldClose(NativeWindow) != 0) : true;
    }

    void FEditorWindow::SetShouldClose(bool bClose) {
        if (NativeWindow) {
            glfwSetWindowShouldClose(NativeWindow, bClose ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    void FEditorWindow::SaveState(const std::string& InIniPath) {
        if (!NativeWindow || InIniPath.empty())
            return;

        bool bMax = IsMaximized();
        int px = 0, py = 0, w = 0, h = 0;
        if (!bMax) {
            glfwGetWindowPos(NativeWindow, &px, &py);
            glfwGetWindowSize(NativeWindow, &w, &h);
        } else {
            w = static_cast<int>(WindowWidth);
            h = static_cast<int>(WindowHeight);
            px = WindowPosX;
            py = WindowPosY;
        }

        std::ofstream out(InIniPath);
        if (out.is_open()) {
            out << "[EditorWindow]\n";
            out << "Width=" << w << "\n";
            out << "Height=" << h << "\n";
            out << "PosX=" << px << "\n";
            out << "PosY=" << py << "\n";
            out << "Maximized=" << (bMax ? 1 : 0) << "\n";
        }
    }

    void FEditorWindow::RestoreState(const std::string& InIniPath) {
        if (!NativeWindow || InIniPath.empty())
            return;

        std::ifstream in(InIniPath);
        if (!in.is_open())
            return;

        std::string line;
        int w = 1600, h = 900, px = 100, py = 100, maxVal = 0;
        while (std::getline(in, line)) {
            if (line.rfind("Width=", 0) == 0)
                w = std::stoi(line.substr(6));
            else if (line.rfind("Height=", 0) == 0)
                h = std::stoi(line.substr(7));
            else if (line.rfind("PosX=", 0) == 0)
                px = std::stoi(line.substr(5));
            else if (line.rfind("PosY=", 0) == 0)
                py = std::stoi(line.substr(5));
            else if (line.rfind("Maximized=", 0) == 0)
                maxVal = std::stoi(line.substr(10));
        }

        if (w > 400 && h > 300) {
            glfwSetWindowSize(NativeWindow, w, h);
            glfwSetWindowPos(NativeWindow, px, py);
        }
        if (maxVal == 1) {
            glfwMaximizeWindow(NativeWindow);
        }
    }

} // namespace Leon::Editor
