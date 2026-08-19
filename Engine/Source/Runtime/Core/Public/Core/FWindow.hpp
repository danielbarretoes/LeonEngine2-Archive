#pragma once

#include "Core/Base.hpp"
#include "Core/events/FEvent.hpp"
#include "RHI/IGraphicsContext.hpp"

#include <functional>
#include <string>

struct GLFWwindow;

namespace Leon {

    /**
     * HD client policy: 16:9, windowed default 720p, hard cap 1080p.
     * Keeps framebuffer memory and camera aspect stable when the OS desktop is 16:10 / 1440p / 4K.
     */
    struct FWindowDisplayPolicy {
        static constexpr unsigned int DefaultWidth = 1280;
        static constexpr unsigned int DefaultHeight = 720;
        static constexpr unsigned int MaxWidth = 1920;
        static constexpr unsigned int MaxHeight = 1080;
        static constexpr int AspectNumerator = 16;
        static constexpr int AspectDenominator = 9;

        static void ConstrainClientSize(unsigned int& InOutWidth, unsigned int& InOutHeight);
    };

    struct FWindowProps {
        std::string Title;
        unsigned int Width;
        unsigned int Height;
        bool bVSync;

        FWindowProps(const std::string& InTitle = "LeonEngine2",
                     unsigned int InWidth = FWindowDisplayPolicy::DefaultWidth,
                     unsigned int InHeight = FWindowDisplayPolicy::DefaultHeight, bool bInVSync = true)
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
        void ApplyHdClientConstraints();

    private:
        GLFWwindow* NativeWindow = nullptr;
        TScope<IGraphicsContext> Context;
        int WindowedPosX = 100;
        int WindowedPosY = 100;
        unsigned int WindowedWidth = FWindowDisplayPolicy::DefaultWidth;
        unsigned int WindowedHeight = FWindowDisplayPolicy::DefaultHeight;

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
