#include "Core/FApplication.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FDebugOverlay.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Renderer/FParticleRenderer.hpp"
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
        FParticleRenderer::Init();
        FTextRenderer::Init();
        FDebugOverlay::Init();
    }

    FApplication::~FApplication() {
        FDebugOverlay::Shutdown();
        FTextRenderer::Shutdown();
        FParticleRenderer::Shutdown();
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
        if (bShift) {
            if (InEvent.GetKeyCode() == Key::F1 && !InEvent.IsRepeat()) {
                bShowGameplayDebug = !bShowGameplayDebug;
                bDebugPhysics = bShowGameplayDebug;
                bDebugCharacter = bShowGameplayDebug;
                FDebugRenderer::SetTraceCaptureEnabled(bShowGameplayDebug);
                LE_CORE_INFO("Gameplay debug (colliders + traces, depth-tested): {0}  [Shift+F1]",
                             bShowGameplayDebug ? "ENABLED" : "DISABLED");
                return true;
            }
            if (InEvent.GetKeyCode() == Key::F5 && !InEvent.IsRepeat()) {
                ToggleDebugAI();
                LE_CORE_INFO("AI debug: {0}  [Shift+F5]", bDebugAI ? "ENABLED" : "DISABLED");
                return true;
            }
            if (InEvent.GetKeyCode() == Key::F6 && !InEvent.IsRepeat()) {
                ToggleDebugPhysics();
                LE_CORE_INFO("Physics debug: {0}  [Shift+F6]", bDebugPhysics ? "ENABLED" : "DISABLED");
                return true;
            }
            if (InEvent.GetKeyCode() == Key::F7 && !InEvent.IsRepeat()) {
                ToggleDebugCharacter();
                return true;
            }
            if (InEvent.GetKeyCode() == Key::F8 && !InEvent.IsRepeat()) {
                ToggleDebugNetwork();
                return true;
            }
        } else {
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
                    FDebugOverlay::Render(AppWindow->GetWidth(), AppWindow->GetHeight(), timestep,
                                          FRenderer::GetStats(), bShowLightGizmos);
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
