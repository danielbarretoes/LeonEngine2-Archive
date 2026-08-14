#pragma once

#include "core/Base.hpp"
#include "core/LayerStack.hpp"
#include "core/Timestep.hpp"
#include "core/Window.hpp"
#include "core/events/ApplicationEvent.hpp"

namespace Leon {

    struct FApplicationProps {
        std::string Name = "LeonEngine App";
        unsigned int WindowWidth = 1280;
        unsigned int WindowHeight = 720;
    };

    using ApplicationProps = FApplicationProps;

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

        FWindow& GetWindow() { return *m_Window; }
        bool IsHUDEnabled() const { return m_bShowHUD; }
        bool IsLightGizmosEnabled() const { return m_bShowLightGizmos; }
        void SetHUDEnabled(bool InbEnabled) { m_bShowHUD = InbEnabled; }
        void SetLightGizmosEnabled(bool InbEnabled) { m_bShowLightGizmos = InbEnabled; }

        static FApplication& Get() { return *s_Instance; }

    private:
        bool OnWindowClose(FWindowCloseEvent& InEvent);
        bool OnWindowResize(FWindowResizeEvent& InEvent);
        bool OnKeyPressed(class FKeyPressedEvent& InEvent);

    private:
        TScope<FWindow> m_Window;
        bool bRunning = true;
        bool bMinimized = false;
        bool m_bShowHUD = false;
        bool m_bShowLightGizmos = false;
        FLayerStack m_LayerStack;
        float m_LastFrameTime = 0.0f;

        static FApplication* s_Instance;
    };

    using Application = FApplication;

    // Client/Sandbox defined entry point
    FApplication* CreateApplication();

} // namespace Leon
