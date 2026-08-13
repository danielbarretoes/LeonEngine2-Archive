#include "engine/core/Application.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/Renderer.hpp"

#include <GLFW/glfw3.h>

namespace Leon {

    FApplication* FApplication::s_Instance = nullptr;

    FApplication::FApplication(const FApplicationProps& InProps) {
        if (s_Instance) {
            LE_CORE_ERROR("Application already exists!");
            return;
        }
        s_Instance = this;

        FLog::Init();
        LE_CORE_INFO("Initializing LeonEngine Application: {0}", InProps.Name);

        m_Window = FWindow::Create(FWindowProps(InProps.Name, InProps.WindowWidth, InProps.WindowHeight));
        m_Window->SetEventCallback(LE_BIND_EVENT_FN(FApplication::OnEvent));

        FRenderer::Init();
    }

    FApplication::~FApplication() {
        FRenderer::Shutdown();
        s_Instance = nullptr;
    }

    void FApplication::PushLayer(FLayer* InLayer) {
        m_LayerStack.PushLayer(InLayer);
    }

    void FApplication::PushOverlay(FLayer* InOverlay) {
        m_LayerStack.PushOverlay(InOverlay);
    }

    void FApplication::Close() {
        bRunning = false;
    }

    void FApplication::OnEvent(FEvent& InEvent) {
        FEventDispatcher dispatcher(InEvent);
        dispatcher.Dispatch<FWindowCloseEvent>(LE_BIND_EVENT_FN(FApplication::OnWindowClose));
        dispatcher.Dispatch<FWindowResizeEvent>(LE_BIND_EVENT_FN(FApplication::OnWindowResize));

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it) {
            if (InEvent.bHandled)
                break;
            (*it)->OnEvent(InEvent);
        }
    }

    bool FApplication::OnWindowClose(FWindowCloseEvent& InEvent) {
        bRunning = false;
        return true;
    }

    bool FApplication::OnWindowResize(FWindowResizeEvent& InEvent) {
        if (InEvent.GetWidth() == 0 || InEvent.GetHeight() == 0) {
            bMinimized = true;
            return false;
        }

        bMinimized = false;
        FRenderer::OnWindowResize(InEvent.GetWidth(), InEvent.GetHeight());
        return false;
    }

    void FApplication::Run() {
        OnInit();

        while (bRunning) {
            float time = (float)glfwGetTime();
            FTimestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;

            if (!bMinimized) {
                OnUpdate(timestep);

                for (FLayer* layer : m_LayerStack) {
                    layer->OnUpdate(timestep);
                }
            }

            m_Window->OnUpdate();
        }

        OnShutdown();
    }

} // namespace Leon
