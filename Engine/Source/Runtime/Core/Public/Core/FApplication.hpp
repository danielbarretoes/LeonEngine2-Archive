#pragma once

#include "Core/Base.hpp"
#include "Core/FLayerStack.hpp"
#include "Core/FTimestep.hpp"
#include "Core/FWindow.hpp"
#include "Core/events/FApplicationEvent.hpp"

#include <chrono>

namespace Leon {

    struct FApplicationCommandLineArgs {
        int Count = 0;
        char** Args = nullptr;

        const char* operator[](int index) const { return Args[index]; }
    };

    struct FApplicationProps {
        std::string Name = "LeonEngine App";
        unsigned int WindowWidth = 1280;
        unsigned int WindowHeight = 720;
        FApplicationCommandLineArgs CommandLineArgs;
    };

    class FApplication {
    public:
        FApplication(const FApplicationProps& InProps = FApplicationProps());
        virtual ~FApplication();

        void Run();
        void Close();

        void PushLayer(FLayer* InLayer);
        void PushOverlay(FLayer* InOverlay);

        virtual void OnInit() {}
        virtual void OnUpdate(FTimestep InTs) {}
        virtual void OnEvent(FEvent& InEvent);
        virtual void OnShutdown() {}

        FWindow& GetWindow() { return *AppWindow; }
        bool IsHUDEnabled() const { return bShowHUD; }
        bool IsLightGizmosEnabled() const { return bShowLightGizmos; }
        void SetHUDEnabled(bool InbEnabled) { bShowHUD = InbEnabled; }
        void SetLightGizmosEnabled(bool InbEnabled) { bShowLightGizmos = InbEnabled; }

        float GetTimeSinceWindowOpenMs() const;
        std::chrono::high_resolution_clock::time_point GetWindowOpenTime() const { return WindowCreationTime; }

        static FApplication& Get() { return *Instance; }
        static bool HasInstance() { return Instance != nullptr; }

    private:
        bool OnWindowClose(FWindowCloseEvent& InEvent);
        bool OnWindowResize(FWindowResizeEvent& InEvent);
        bool OnKeyPressed(class FKeyPressedEvent& InEvent);

    private:
        TScope<FWindow> AppWindow;
        std::chrono::high_resolution_clock::time_point WindowCreationTime;
        bool bRunning = true;
        bool bMinimized = false;
        bool bShowHUD = false;
        bool bShowLightGizmos = false;
        FLayerStack LayerStack;
        float LastFrameTime = 0.0f;

        static FApplication* Instance;
    };

    // Project-defined entry point (CreateApplication)
    FApplication* CreateApplication(FApplicationCommandLineArgs InArgs = {});

} // namespace Leon
