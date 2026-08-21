#include "Engine/FGameplaySession.hpp"
#include "Core/FLog.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/AWorldSettings.hpp"
#include "Gameplay/UClassRegistry.hpp"

namespace Leon {

    namespace {
        bool IsStockEngineGameMode(const std::string& InClassName) {
            return InClassName == "AGameModeBase" || InClassName == "AGameMode" || InClassName.empty();
        }
    } // namespace

    std::string FGameplaySession::ResolveGameModeClassFromWorld(UWorld& InWorld, const std::string& InFallback) {
        for (auto& ActorRef : InWorld.GetAllActors()) {
            if (!ActorRef)
                continue;
            const FWorldSettingsComponent* Ws = nullptr;
            if (auto* WorldSettings = dynamic_cast<AWorldSettings*>(ActorRef.get()))
                Ws = &WorldSettings->GetWorldSettings();
            else if (ActorRef->HasComponent<FWorldSettingsComponent>())
                Ws = &ActorRef->GetComponent<FWorldSettingsComponent>();
            if (Ws && !Ws->GameModeClass.empty())
                return Ws->GameModeClass;
        }
        return InFallback.empty() ? "AGameModeBase" : InFallback;
    }

    bool FGameplaySession::Start(UWorld& InWorld, const FGameplaySessionParams& InParams) {
        InWorld.SetNetMode(InParams.NetMode);

        if (InParams.bSpawnGameMode && InParams.NetMode != ENetMode::Client) {
            FGameModeConfig Config = InParams.GameModeConfig;
            Config.GameModeClass = ResolveGameModeClassFromWorld(InWorld, Config.GameModeClass);

            AGameModeBase* GameMode = nullptr;
            if (!Config.GameModeClass.empty() && UClassRegistry::Get().HasClass(Config.GameModeClass)) {
                GameMode = dynamic_cast<AGameModeBase*>(
                    UClassRegistry::Get().CreateActorOfClass(Config.GameModeClass, &InWorld, "GameMode"));
            }
            if (!GameMode) {
                LE_CORE_WARN("FGameplaySession: Could not create '{}', falling back to AGameModeBase",
                             Config.GameModeClass);
                GameMode = InWorld.SpawnActor<AGameModeBase>("GameMode");
            }

            if (GameMode) {
                // Unreal-like: stock GameModeBase gets DefaultPawn fly camera from config.
                // Project GameModes keep constructor defaults (do not clobber with INI/stock pawn).
                if (IsStockEngineGameMode(GameMode->GetClass())) {
                    GameMode->DefaultPawnClass =
                        Config.DefaultPawnClass.empty() ? "ADefaultPawn" : Config.DefaultPawnClass;
                    GameMode->PlayerControllerClass = Config.PlayerControllerClass.empty()
                                                          ? "APlayerController"
                                                          : Config.PlayerControllerClass;
                    GameMode->HUDClass = Config.HUDClass.empty() ? "AHUD" : Config.HUDClass;
                    GameMode->GameStateClass =
                        Config.GameStateClass.empty() ? "AGameStateBase" : Config.GameStateClass;
                    GameMode->PlayerStateClass =
                        Config.PlayerStateClass.empty() ? "APlayerState" : Config.PlayerStateClass;
                }
                GameMode->MaxPlayers = InParams.MaxPlayers;
                InWorld.SetGameMode(GameMode);
                LE_CORE_INFO("FGameplaySession: Spawned GameMode '{}' (Pawn='{}', PC='{}', HUD='{}')",
                             GameMode->GetClass(), GameMode->DefaultPawnClass, GameMode->PlayerControllerClass,
                             GameMode->HUDClass);
            } else {
                LE_CORE_ERROR("FGameplaySession: Failed to spawn GameMode");
                return false;
            }
        }

        if (InParams.bInitWorld)
            InWorld.InitWorld();
        if (InParams.bBeginPlay)
            InWorld.BeginPlay();

        // DefaultPawn fly camera expects GameOnly input (Unreal PIE).
        if (APlayerController* Pc = InWorld.GetFirstPlayerController()) {
            if (Pc->GetPawn() && Pc->GetPawn()->GetClass() == "ADefaultPawn")
                Pc->SetInputModeGameOnly();
        }
        return true;
    }

    void FGameplaySession::Stop(UWorld& InWorld) {
        if (InWorld.HasBegunPlay())
            InWorld.EndPlay();
        InWorld.Clear();
    }

} // namespace Leon
