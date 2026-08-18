#include "Engine/UEngine.hpp"
#include "FGameViewportLayer.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"
#include "Assets/FAssetPath.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FRenderCommand.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Audio/FAudioDevice.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <utility>

namespace Leon {

    void UEngine::RequestTravel(const std::string& InLevelName) {
        PendingTravelMap = InLevelName;
        bPendingTravel = true;
        LE_CORE_INFO("UEngine: Travel requested to '{0}'", InLevelName);
    }

    void UEngine::ProcessPendingTravel() {
        if (!bPendingTravel)
            return;
        bPendingTravel = false;
        std::string target = PendingTravelMap;
        PendingTravelMap.clear();
        TravelToMap(target);
    }

    bool UEngine::TravelToMap(const std::string& InVirtualMapPath) {
        LE_CORE_INFO("UEngine: Traveling to '{0}'...", InVirtualMapPath);

        // Flow: load into a new UWorld first. Only on success: EndPlay+release old world, rebind, InitWorld/BeginPlay.
        auto newWorld = UWorld::Create("MainWorld");
        newWorld->SetProjectRendererDefaults(ProjectShadowMapResolution, bProjectEnablePlanarReflection,
                                             ProjectCascadeCount, ProjectShadowDistance, ProjectPlanarReflectionQuality,
                                             ProjectPlanarReflectionResolutionScale);
        if (GameInstance)
            newWorld->SetNetMode(GameInstance->GetNetMode());

        if (!LoadMapIntoWorld(newWorld, InVirtualMapPath)) {
            LE_CORE_ERROR("UEngine: Travel aborted; keeping current world (map '{0}' failed to load)",
                          InVirtualMapPath);
            return false;
        }

        if (ActiveWorld) {
            ActiveWorld->EndPlay();
            ActiveWorld->Clear();
        }

        if (ViewportLayer) {
            FOnScreenDebugMessageManager::Get().Clear();
            FUIRenderer::Shutdown();
        }

        ActiveWorld = newWorld;
        CurrentMapName = InVirtualMapPath;
        if (GameInstance) {
            GameInstance->SetWorld(ActiveWorld);
            GameInstance->SetTravelURL(InVirtualMapPath);
        }
        if (ViewportLayer) {
            ViewportLayer->SetWorld(ActiveWorld);
        }

        UAssetManager::UnloadUnused();

        AGameModeBase* gameMode = nullptr;
        if (ActiveWorld->GetNetMode() != ENetMode::Client) {
            if (!GameModeConfig.GameModeClass.empty() && UClassRegistry::Get().HasClass(GameModeConfig.GameModeClass)) {
                gameMode = dynamic_cast<AGameModeBase*>(UClassRegistry::Get().CreateActorOfClass(
                    GameModeConfig.GameModeClass, ActiveWorld.get(), "GameMode"));
            }
            if (!gameMode) {
                gameMode = ActiveWorld->SpawnActor<AGameModeBase>("GameMode");
            }
            ApplyGameModeConfig(gameMode);
            ActiveWorld->SetGameMode(gameMode);
        }

        if (ViewportLayer)
            FUIRenderer::Init();
        ActiveWorld->InitWorld();
        ActiveWorld->BeginPlay();

        if (ViewportLayer && FApplication::HasInstance()) {
            auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() > 0 && window.GetHeight() > 0) {
                float aspect = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
                APlayerController* pc = ActiveWorld->GetFirstPlayerController();
                if (pc && pc->GetPlayerCameraManager()) {
                    pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                }
                if (FWorldRenderer* renderer = ActiveWorld->GetWorldRenderer()) {
                    renderer->OnViewportResize(window.GetWidth(), window.GetHeight());
                }
            }
        }

        LE_CORE_INFO("UEngine: Travel complete — map '{0}' is live", InVirtualMapPath);
        return true;
    }

} // namespace Leon
