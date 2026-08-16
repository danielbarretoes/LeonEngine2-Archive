#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FDebugOverlay.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "RHI/FRenderer.hpp"
#include "Renderer/FTextRenderer.hpp"

#include <GLFW/glfw3.h>

namespace Leon {

    FApplication* FApplication::Instance = nullptr;

    FApplication::FApplication(const FApplicationProps& InProps) {
        if (Instance) {
            LE_CORE_ERROR("FApplication already exists!");
            return;
        }
        Instance = this;

        FLog::Init();
        LE_CORE_INFO("Initializing LeonEngine FApplication: {0}", InProps.Name);

        AppWindow = FWindow::Create(FWindowProps(InProps.Name, InProps.WindowWidth, InProps.WindowHeight));
        WindowCreationTime = std::chrono::high_resolution_clock::now();
        AppWindow->SetEventCallback(LE_BIND_EVENT_FN(FApplication::OnEvent));

        FRenderer::Init();
        UAssetManager::Init();
        FDebugRenderer::Init();
        FTextRenderer::Init();
        FDebugOverlay::Init();
    }

    FApplication::~FApplication() {
        FDebugOverlay::Shutdown();
        FTextRenderer::Shutdown();
        FDebugRenderer::Shutdown();
        UAssetManager::Shutdown();
        FRenderer::Shutdown();
        Instance = nullptr;
    }

    void FApplication::PushLayer(FLayer* InLayer) {
        LayerStack.PushLayer(InLayer);
    }

    void FApplication::PushOverlay(FLayer* InOverlay) {
        LayerStack.PushOverlay(InOverlay);
    }

    void FApplication::Close() {
        bRunning = false;
    }

    void FApplication::OnEvent(FEvent& InEvent) {
        FEventDispatcher dispatcher(InEvent);
        dispatcher.Dispatch<FWindowCloseEvent>(LE_BIND_EVENT_FN(FApplication::OnWindowClose));
        dispatcher.Dispatch<FWindowResizeEvent>(LE_BIND_EVENT_FN(FApplication::OnWindowResize));
        dispatcher.Dispatch<FKeyPressedEvent>(LE_BIND_EVENT_FN(FApplication::OnKeyPressed));

        for (auto it = LayerStack.rbegin(); it != LayerStack.rend(); ++it) {
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
        bool bShift = FInput::IsKeyPressed(Key::LeftShift) || FInput::IsKeyPressed(Key::RightShift);
        if (!bShift) {
            if (InEvent.GetKeyCode() == Key::F1 && !InEvent.IsRepeat()) {
                bShowHUD = !bShowHUD;
                LE_CORE_INFO("Diagnostics HUD: {0}", bShowHUD ? "ENABLED" : "DISABLED");
                return true;
            }

            if (InEvent.GetKeyCode() == Key::F2 && !InEvent.IsRepeat()) {
                bShowLightGizmos = !bShowLightGizmos;
                LE_CORE_INFO("Light Debug Gizmos: {0}", bShowLightGizmos ? "ENABLED" : "DISABLED");
                return true;
            }
        }

        return false;
    }

    void FApplication::Run() {
        OnInit();

        while (bRunning) {
            float time = (float)glfwGetTime();
            FTimestep timestep = time - LastFrameTime;
            LastFrameTime = time;

            if (!bMinimized) {
                OnUpdate(timestep);

                for (FLayer* layer : LayerStack) {
                    layer->OnUpdate(timestep);
                }

                // Render Top-Level Diagnostics HUD Overlay if enabled
                if (bShowHUD) {
                    FDebugOverlay::Render(AppWindow->GetWidth(), AppWindow->GetHeight(), timestep, FRenderer::GetStats(),
                                          bShowLightGizmos);
                }
            }

            AppWindow->OnUpdate();
        }

        OnShutdown();
    }

    float FApplication::GetTimeSinceWindowOpenMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<float, std::milli>(now - WindowCreationTime).count();
    }

} // namespace Leon
