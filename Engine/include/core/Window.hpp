#pragma once

#include "core/Base.hpp"
#include "core/events/Event.hpp"
#include "renderer/GraphicsContext.hpp"

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

    using WindowProps = FWindowProps;

    class FWindow {
    public:
        using FEventCallbackFn = std::function<void(FEvent&)>;

        FWindow(const FWindowProps& InProps);
        ~FWindow();

        void OnUpdate();

        unsigned int GetWidth() const { return m_Data.Width; }
        unsigned int GetHeight() const { return m_Data.Height; }

        void SetEventCallback(const FEventCallbackFn& InCallback) { m_Data.EventCallback = InCallback; }
        void SetVSync(bool bInEnabled);
        bool IsVSync() const { return m_Data.bVSync; }

        GLFWwindow* GetNativeWindow() const { return m_Window; }

        static TScope<FWindow> Create(const FWindowProps& InProps = FWindowProps());

    private:
        void Init(const FWindowProps& InProps);
        void Shutdown();

    private:
        GLFWwindow* m_Window = nullptr;
        TScope<IGraphicsContext> m_Context;

        struct FWindowData {
            std::string Title;
            unsigned int Width = 0;
            unsigned int Height = 0;
            bool bVSync = true;
            FEventCallbackFn EventCallback;
        };

        FWindowData m_Data;
    };

    using Window = FWindow;

} // namespace Leon
