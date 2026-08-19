#pragma once

#include "Engine/UEngine.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FRenderCommand.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Renderer/FRenderDebugHotkeys.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Audio/FAudioDevice.hpp"
#include "Engine/Components.hpp"

namespace Leon {

    /**
     * @brief Active game viewport layer: ticks world, renders scene + debug gizmos + HUD/UI, dispatches UI input.
     *
     * Render order per frame:
     *   World 3D → Light gizmos (F2) → AHUD / UUserWidget / PrintString → (FApplication) F1 DebugOverlay → Present
     */
    class FGameViewportLayer : public FLayer {
    public:
        explicit FGameViewportLayer(const TRef<UWorld>& InWorld) : FLayer("GameViewportLayer"), World(InWorld) {}

        void SetWorld(const TRef<UWorld>& InWorld) { World = InWorld; }
        TRef<UWorld> GetWorld() const { return World; }

        void OnAttach() override {
            LE_CORE_INFO("FGameViewportLayer: Attached to active World '{0}'", World ? World->GetName() : "None");
            FUIRenderer::Init();
            UpdateCameraAspect();
            ApplyCursorFromPlayerController();
        }

        void OnDetach() override { LE_CORE_INFO("FGameViewportLayer: Detached from World"); }

        void OnUpdate(FTimestep InTs) override {
            UEngine::Get().ProcessPendingTravel();

            if (!World)
                return;

            FFrameProfiler::BeginFrame();
            FOnScreenDebugMessageManager::Get().Tick(InTs.GetSeconds());
            FAudioDevice::Get().Tick(InTs.GetSeconds());

            {
                FFrameProfiler::FScope game(&FFrameProfiler::Working().GameMs);
                World->Tick(InTs);
            }

            ApplyCursorFromPlayerController();
            DispatchUIMouseMove();

            FPerspectiveCamera activeCamera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            APlayerController* pc = World->GetFirstPlayerController();
            if (pc) {
                pc->GetPlayerViewPoint(activeCamera);
            }
            {
                const glm::vec3 loc = activeCamera.GetPosition();
                const glm::vec3 fwd = activeCamera.GetForwardDirection();
                FAudioDevice::Get().SetListener(loc, fwd, glm::vec3(0.0f, 1.0f, 0.0f));
            }

            {
                FFrameProfiler::FScope render(&FFrameProfiler::Working().RenderMs);
                World->OnRender(activeCamera);
            }
            DrawLightGizmos(activeCamera);
            {
                FFrameProfiler::FScope ui(&FFrameProfiler::Working().UIMs);
                DrawHUDAndUI();
            }
            FFrameProfiler::EndFrame(InTs.GetMilliseconds());
        }

        void OnEvent(FEvent& InEvent) override {
            FEventDispatcher dispatcher(InEvent);

            dispatcher.Dispatch<FWindowResizeEvent>([this](FWindowResizeEvent& e) {
                if (e.GetWidth() > 0 && e.GetHeight() > 0) {
                    float aspect = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
                    if (World) {
                        APlayerController* pc = World->GetFirstPlayerController();
                        if (pc && pc->GetPlayerCameraManager()) {
                            pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                        }
                        if (FWorldRenderer* renderer = World->GetWorldRenderer()) {
                            renderer->OnViewportResize(e.GetWidth(), e.GetHeight());
                        }
                    }
                }
                return false;
            });

            dispatcher.Dispatch<FMouseButtonPressedEvent>(
                [this](FMouseButtonPressedEvent& e) { return HandleUIMouseButton(e.GetMouseButton(), true); });

            dispatcher.Dispatch<FMouseButtonReleasedEvent>(
                [this](FMouseButtonReleasedEvent& e) { return HandleUIMouseButton(e.GetMouseButton(), false); });

            dispatcher.Dispatch<FMouseScrolledEvent>([this](FMouseScrolledEvent& e) {
                return HandleUIMouseWheel(e.GetYOffset());
            });

            dispatcher.Dispatch<FKeyPressedEvent>([this](FKeyPressedEvent& e) {
                if (e.IsRepeat() || !World)
                    return false;

                // F1/F2 and Shift+F* are owned by FApplication. F3–F12: FRenderDebugHotkeys.
                auto* renderer = World->GetWorldRenderer();
                if (!renderer)
                    return false;
                return FRenderDebugHotkeys::ApplyKey(*renderer, e.GetKeyCode());
            });
        }

    private:
        void UpdateCameraAspect() {
            if (!FApplication::HasInstance())
                return;
            auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() == 0 || window.GetHeight() == 0 || !World)
                return;
            float aspect = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
            APlayerController* pc = World->GetFirstPlayerController();
            if (pc && pc->GetPlayerCameraManager()) {
                pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
            }
        }

        void ApplyCursorFromPlayerController() {
            if (!World || !FApplication::HasInstance())
                return;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc)
                return;

            bool bShowCursor = pc->ShouldShowMouseCursor();
            if (pc->GetInputMode() == EInputMode::GameOnly) {
                bShowCursor = false;
            }
            FApplication::Get().GetWindow().SetCursorVisible(bShowCursor);
        }

        void DispatchUIMouseMove() {
            if (!World)
                return;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc || !pc->IsUIInputAllowed())
                return;

            AHUD* hud = pc->GetHUD();
            if (!hud)
                return;

            auto [mx, my] = FInput::GetMousePosition();
            hud->OnMouseMove({mx, my});
        }

        bool HandleUIMouseButton(int InButton, bool bPressed) {
            if (!World)
                return false;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc || !pc->IsUIInputAllowed())
                return false;

            AHUD* hud = pc->GetHUD();
            if (!hud)
                return false;

            auto [mx, my] = FInput::GetMousePosition();
            glm::vec2 pos{mx, my};
            if (bPressed) {
                return hud->OnMouseButtonDown(InButton, pos);
            }
            return hud->OnMouseButtonUp(InButton, pos);
        }

        bool HandleUIMouseWheel(float InWheelDelta) {
            if (!World)
                return false;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc || !pc->IsUIInputAllowed())
                return false;
            AHUD* hud = pc->GetHUD();
            if (!hud)
                return false;
            auto [mx, my] = FInput::GetMousePosition();
            return hud->OnMouseWheel(InWheelDelta, {mx, my});
        }

        void DrawLightGizmos(const FPerspectiveCamera& InCamera) {
            if (!World || !FApplication::HasInstance())
                return;
            const bool bGizmos = FApplication::Get().IsLightGizmosEnabled();
            if (!bGizmos)
                return;

            // Light gizmos overlay the final image (no scene depth after post-process).
            FDebugRenderer::BeginScene(InCamera);
            auto& reg = World->GetRegistry();

            auto dirView = reg.view<FDirectionalLightComponent, FTransformComponent>();
            for (auto entity : dirView) {
                auto [dirComp, transform] = dirView.get<FDirectionalLightComponent, FTransformComponent>(entity);
                if (dirComp.bEnabled) {
                    FDebugRenderer::DrawDirectionalLightGizmo(dirComp.Light, transform.Translation, 2.5f);
                }
            }

            auto pointView = reg.view<FPointLightComponent, FTransformComponent>();
            for (auto entity : pointView) {
                auto [pointComp, transform] = pointView.get<FPointLightComponent, FTransformComponent>(entity);
                if (pointComp.bEnabled) {
                    FDebugRenderer::DrawPointLightGizmo(pointComp.Light);
                }
            }

            auto spotView = reg.view<FSpotLightComponent, FTransformComponent>();
            for (auto entity : spotView) {
                auto [spotComp, transform] = spotView.get<FSpotLightComponent, FTransformComponent>(entity);
                if (spotComp.bEnabled) {
                    FDebugRenderer::DrawSpotLightGizmo(spotComp.Light);
                }
            }

            // Overlay gizmos intentionally ignore depth so they stay readable on screen.
            FDebugRenderer::EndScene(false);
        }

        void DrawHUDAndUI() {
            if (!World)
                return;

            APlayerController* pc = World->GetFirstPlayerController();
            if (pc) {
                if (AHUD* hud = pc->GetHUD()) {
                    hud->DrawHUD();
                    return;
                }
            }

            if (!FApplication::HasInstance())
                return;
            auto& window = FApplication::Get().GetWindow();
            uint32_t w = window.GetWidth();
            uint32_t h = window.GetHeight();
            if (w == 0 || h == 0)
                return;

            FUIRenderer::Begin(w, h);
            FOnScreenDebugMessageManager::Get().Draw(static_cast<float>(w), static_cast<float>(h));
            FUIRenderer::End();
        }

        TRef<UWorld> World;
    };


} // namespace Leon
