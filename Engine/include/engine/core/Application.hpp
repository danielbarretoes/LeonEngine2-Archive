#pragma once

#include "engine/core/Base.hpp"
#include "engine/core/LayerStack.hpp"
#include "engine/core/Timestep.hpp"
#include "engine/core/Window.hpp"
#include "engine/core/events/ApplicationEvent.hpp"

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

        static FApplication& Get() { return *s_Instance; }

    private:
        bool OnWindowClose(FWindowCloseEvent& InEvent);
        bool OnWindowResize(FWindowResizeEvent& InEvent);

    private:
        TScope<FWindow> m_Window;
        bool bRunning = true;
        bool bMinimized = false;
        FLayerStack m_LayerStack;
        float m_LastFrameTime = 0.0f;

        static FApplication* s_Instance;
    };

    using Application = FApplication;

    // Client/Sandbox defined entry point
    FApplication* CreateApplication();

} // namespace Leon
