#pragma once

#include "Core/Base.hpp"
#include "Core/events/FEvent.hpp"
#include "RHI/IGraphicsContext.hpp"

#include <functional>
#include <string>

struct GLFWwindow;

namespace Leon {

    struct FWindowProps {
        std::string Title;
        unsigned int Width;
        unsigned int Height;
        bool bVSync;

        FWindowProps(const std::string& InTitle = "LeonEngine2", unsigned int InWidth = 1280,
                     unsigned int InHeight = 720, bool bInVSync = true)
            : Title(InTitle), Width(InWidth), Height(InHeight), bVSync(bInVSync) {}
    };

    class FWindow {
    public:
        using FEventCallbackFn = std::function<void(FEvent&)>;

        FWindow(const FWindowProps& InProps);
        ~FWindow();

        void OnUpdate();

        unsigned int GetWidth() const { return Data.Width; }
        unsigned int GetHeight() const { return Data.Height; }

        void SetEventCallback(const FEventCallbackFn& InCallback) { Data.EventCallback = InCallback; }
        void SetVSync(bool bInEnabled);
        bool IsVSync() const { return Data.bVSync; }

        void SetFullscreen(bool bInEnabled);
        bool IsFullscreen() const { return Data.bFullscreen; }

        void SetCursorVisible(bool bVisible);
        bool IsCursorVisible() const { return Data.bCursorVisible; }

        GLFWwindow* GetNativeWindow() const { return NativeWindow; }

        static TScope<FWindow> Create(const FWindowProps& InProps = FWindowProps());

    private:
        void Init(const FWindowProps& InProps);
        void Shutdown();

    private:
        GLFWwindow* NativeWindow = nullptr;
        TScope<IGraphicsContext> Context;

        struct FWindowData {
            std::string Title;
            unsigned int Width = 0;
            unsigned int Height = 0;
            bool bVSync = true;
            bool bFullscreen = false;
            bool bCursorVisible = true;
            FEventCallbackFn EventCallback;
        };

        FWindowData Data;
    };

} // namespace Leon
