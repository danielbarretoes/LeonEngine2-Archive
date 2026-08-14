#include "core/Application.hpp"
#include "core/Input.hpp"
#include "core/Log.hpp"
#include "core/events/KeyEvent.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/DebugOverlay.hpp"
#include "renderer/DebugRenderer.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/TextRenderer.hpp"

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
        m_WindowCreationTime = std::chrono::high_resolution_clock::now();
        m_Window->SetEventCallback(LE_BIND_EVENT_FN(FApplication::OnEvent));

        FRenderer::Init();
        FAssetManager::Init();
        FDebugRenderer::Init();
        FTextRenderer::Init();
        FDebugOverlay::Init();
    }

    FApplication::~FApplication() {
        FDebugOverlay::Shutdown();
        FTextRenderer::Shutdown();
        FDebugRenderer::Shutdown();
        FAssetManager::Shutdown();
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
        dispatcher.Dispatch<FKeyPressedEvent>(LE_BIND_EVENT_FN(FApplication::OnKeyPressed));

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

    bool FApplication::OnKeyPressed(FKeyPressedEvent& InEvent) {
        if (InEvent.GetKeyCode() == Key::F1 && !InEvent.IsRepeat()) {
            m_bShowHUD = !m_bShowHUD;
            LE_CORE_INFO("Diagnostics HUD: {0}", m_bShowHUD ? "ENABLED" : "DISABLED");
            return false;
        }

        if (InEvent.GetKeyCode() == Key::F2 && !InEvent.IsRepeat()) {
            m_bShowLightGizmos = !m_bShowLightGizmos;
            LE_CORE_INFO("Light Debug Gizmos: {0}", m_bShowLightGizmos ? "ENABLED" : "DISABLED");
            return false;
        }

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

                // Render Top-Level Diagnostics HUD Overlay if enabled
                if (m_bShowHUD) {
                    FDebugOverlay::Render(m_Window->GetWidth(), m_Window->GetHeight(), timestep, FRenderer::GetStats(),
                                          m_bShowLightGizmos);
                }
            }

            m_Window->OnUpdate();
        }

        OnShutdown();
    }

    float FApplication::GetTimeSinceWindowOpenMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<float, std::milli>(now - m_WindowCreationTime).count();
    }

} // namespace Leon
