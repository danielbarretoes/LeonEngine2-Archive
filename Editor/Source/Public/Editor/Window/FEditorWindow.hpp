#pragma once

#include "Core/Base.hpp"
#include "Core/events/FEvent.hpp"
#include "RHI/IGraphicsContext.hpp"

#include <functional>
#include <string>

struct GLFWwindow;

namespace Leon::Editor {

    /**
     * @brief Editor-specific window configuration.
     */
    struct FEditorWindowProps {
        std::string Title = "Leon Engine Editor";
        unsigned int Width = 1600;
        unsigned int Height = 900;
        bool bMaximized = false;
        bool bVSync = true;

        FEditorWindowProps() = default;
        FEditorWindowProps(const std::string& InTitle, unsigned int InWidth, unsigned int InHeight,
                           bool bInMaximized = false, bool bInVSync = true)
            : Title(InTitle), Width(InWidth), Height(InHeight), bMaximized(bInMaximized), bVSync(bInVSync) {}
    };

    /**
     * @brief Dedicated Editor Window class, decoupled from game runtime client window policies.
     * Manages desktop editor window lifecycle, maximize/restore states, RHI graphics context,
     * and editor-specific geometry persistence.
     */
    class FEditorWindow {
    public:
        using FEventCallbackFn = std::function<void(FEvent&)>;

        explicit FEditorWindow(const FEditorWindowProps& InProps = FEditorWindowProps());
        ~FEditorWindow();

        void OnUpdate();

        unsigned int GetWidth() const { return WindowWidth; }
        unsigned int GetHeight() const { return WindowHeight; }

        void SetEventCallback(const FEventCallbackFn& InCallback) { EventCallback = InCallback; }
        void SetVSync(bool bInEnabled);
        bool IsVSync() const { return bVSync; }

        void Maximize();
        void Restore();
        bool IsMaximized() const;

        void SetTitle(const std::string& InTitle);
        void UpdateEditorTitle(const std::string& InProjectName, const std::string& InMapName);

        void SaveState(const std::string& InIniPath);
        void RestoreState(const std::string& InIniPath);

        GLFWwindow* GetNativeWindow() const { return NativeWindow; }
        bool ShouldClose() const;
        void SetShouldClose(bool bClose);

    private:
        void Init(const FEditorWindowProps& InProps);
        void Shutdown();

        GLFWwindow* NativeWindow = nullptr;
        TScope<IGraphicsContext> Context;

        unsigned int WindowWidth = 1600;
        unsigned int WindowHeight = 900;
        int WindowPosX = 100;
        int WindowPosY = 100;
        std::string BaseTitle;
        bool bVSync = true;
        FEventCallbackFn EventCallback;
    };

} // namespace Leon::Editor
