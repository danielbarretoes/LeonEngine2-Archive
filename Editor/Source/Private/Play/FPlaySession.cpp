#include "Editor/Play/FPlaySession.hpp"
#include "Core/FLog.hpp"
#include "Editor/Play/FGameModuleLoader.hpp"
#include "Engine/FGameplaySession.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/UIpNetDriver.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Audio/FAudioDevice.hpp"

#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Leon::Editor {

    namespace {
        /**
         * PIE GameMode resolution (Unreal-like):
         * 1) WorldSettings.GameModeClass (applied later in FGameplaySession)
         * 2) .lproject DefaultGameMode
         * 3) AGameModeBase + ADefaultPawn (engine default fly camera)
         *
         * Packaged game still uses DefaultEngine/DefaultGame.ini via UEngine::BuildGameModeConfig.
         */
        FGameModeConfig BuildPieGameModeConfig(const FProjectDescriptor& InProject) {
            FGameModeConfig Config;
            Config.GameModeClass =
                InProject.DefaultGameMode.empty() ? "AGameModeBase" : InProject.DefaultGameMode;
            Config.DefaultPawnClass = "ADefaultPawn";
            Config.PlayerControllerClass = "APlayerController";
            Config.HUDClass = "AHUD";
            Config.GameStateClass = "AGameStateBase";
            Config.PlayerStateClass = "APlayerState";
            return Config;
        }
    } // namespace

    ENetMode FPlaySession::ToEngineNetMode(EPlayNetMode InMode) const {
        switch (InMode) {
        case EPlayNetMode::ListenServer:
            return ENetMode::ListenServer;
        case EPlayNetMode::Client:
            return ENetMode::Client;
        case EPlayNetMode::DedicatedServer:
            return ENetMode::DedicatedServer;
        case EPlayNetMode::Standalone:
        default:
            return ENetMode::Standalone;
        }
    }

    bool FPlaySession::Start(const FPlaySettings& InSettings, const FProjectDescriptor& InProject,
                             const std::string& InMapDiskPath, const std::string& InMapName) {
        if (bPlaying) {
            if (LogFn)
                LogFn("Play session already active", true);
            return false;
        }

        FPlaySettings Settings = InSettings;
        Settings.Clamp();
        if (Settings.NumberOfPlayers > 1 && Settings.NetMode == EPlayNetMode::Standalone)
            Settings.NetMode = EPlayNetMode::ListenServer;

        if (Settings.bAutoSaveMapBeforePlay && SaveMapFn)
            SaveMapFn();

        if (InMapDiskPath.empty() || !std::filesystem::exists(InMapDiskPath)) {
            if (LogFn)
                LogFn("Cannot Play: no map is loaded on disk", true);
            return false;
        }

        PlayWorld = UWorld::Create("PlayWorld");
        if (!PlayWorld) {
            if (LogFn)
                LogFn("Failed to create PlayWorld", true);
            return false;
        }

        FMapSerializer Serializer(PlayWorld);
        if (!Serializer.Deserialize(InMapDiskPath)) {
            if (LogFn)
                LogFn("Failed to load map into PlayWorld", true);
            PlayWorld.reset();
            return false;
        }

        // Prefer project GameInstance (e.g. ULeonTournamentGameInstance) from the loaded game module.
        if (FGameModuleLoader::IsLoaded() && FGameModuleLoader::GetHooks().GameInstanceFactory) {
            PlayGameInstance = FGameModuleLoader::GetHooks().GameInstanceFactory();
        }
        if (!PlayGameInstance)
            PlayGameInstance = CreateRef<UGameInstance>("PIE_GameInstance");
        PlayGameInstance->Init();
        PlayGameInstance->SetWorld(PlayWorld);
        PlayGameInstance->SetNetMode(ToEngineNetMode(Settings.NetMode));
        PlayWorld->SetNetMode(ToEngineNetMode(Settings.NetMode));

        FUIRenderer::Init();
        FAudioDevice::Get().Init();

        FGameplaySessionParams Params;
        Params.GameModeConfig = BuildPieGameModeConfig(InProject);
        Params.NetMode = ToEngineNetMode(Settings.NetMode);
        Params.MaxPlayers = Settings.NumberOfPlayers;
        if (Settings.NetMode == EPlayNetMode::Client)
            Params.bSpawnGameMode = false;

        const std::string ResolvedGm =
            FGameplaySession::ResolveGameModeClassFromWorld(*PlayWorld, Params.GameModeConfig.GameModeClass);
        LE_CORE_INFO("FPlaySession: Starting PIE map='{}' GameMode='{}' Pawn='{}' PC='{}' HUD='{}'", InMapName,
                     ResolvedGm, Params.GameModeConfig.DefaultPawnClass, Params.GameModeConfig.PlayerControllerClass,
                     Params.GameModeConfig.HUDClass);

        if (!FGameplaySession::Start(*PlayWorld, Params)) {
            if (LogFn)
                LogFn("FGameplaySession::Start failed", true);
            PlayWorld.reset();
            PlayGameInstance.reset();
            return false;
        }

        if (AGameModeBase* Gm = PlayWorld->GetGameMode()) {
            LE_CORE_INFO("FPlaySession: Active GameMode class='{}' DefaultPawn='{}'", Gm->GetClass(),
                         Gm->DefaultPawnClass);
        }

        ActiveSettings = Settings;
        bPlaying = true;
        bStopRequested = false;

        if (Settings.NetMode == EPlayNetMode::ListenServer || Settings.NetMode == EPlayNetMode::DedicatedServer) {
            if (!StartListenIfNeeded()) {
                if (LogFn)
                    LogFn("Listen server failed to start", true);
            }
        } else if (Settings.NetMode == EPlayNetMode::Client) {
            if (!PlayGameInstance->ConnectToHost(PlayWorld.get(), Settings.ClientAddress,
                                                 static_cast<uint16_t>(Settings.ListenPort))) {
                if (LogFn)
                    LogFn("Failed to connect PIE client to host", true);
            }
        }

        if (Settings.NumberOfPlayers > 1 && Settings.NetMode != EPlayNetMode::Client)
            SpawnExtraClients();

        if (LogFn) {
            LogFn(std::string("PIE started: ") + InMapName + " [" + ResolvedGm + "]", false);
        }
        return true;
    }

    bool FPlaySession::StartListenIfNeeded() {
        if (!PlayGameInstance)
            return false;
        return PlayGameInstance->StartListenServer(PlayWorld.get(), static_cast<uint16_t>(ActiveSettings.ListenPort));
    }

    bool FPlaySession::SpawnExtraClients() {
        if (!SpawnClientFn)
            return false;
        bool bOk = true;
        for (int i = 1; i < ActiveSettings.NumberOfPlayers; ++i) {
            if (!SpawnClientFn(ActiveSettings, i))
                bOk = false;
        }
        return bOk;
    }

    void FPlaySession::Stop() {
        if (!bPlaying)
            return;

#ifdef _WIN32
        for (void* Handle : ChildProcessHandles) {
            if (Handle) {
                TerminateProcess(static_cast<HANDLE>(Handle), 0);
                CloseHandle(static_cast<HANDLE>(Handle));
            }
        }
#endif
        ChildProcessHandles.clear();

        if (PlayGameInstance) {
            PlayGameInstance->ShutdownNetDriver();
        }

        if (PlayWorld) {
            FGameplaySession::Stop(*PlayWorld);
            PlayWorld.reset();
        }
        PlayGameInstance.reset();
        bPlaying = false;
        bStopRequested = false;
        LE_CORE_INFO("FPlaySession: Stopped");
        if (LogFn)
            LogFn("PIE stopped", false);
    }

    void FPlaySession::Tick(float InDeltaSeconds) {
        if (!bPlaying)
            return;
        if (bStopRequested) {
            Stop();
            return;
        }
        if (!PlayWorld)
            return;

        if (PlayGameInstance && PlayGameInstance->GetIpNetDriver())
            PlayGameInstance->GetIpNetDriver()->Tick(InDeltaSeconds);

        PlayWorld->Tick(InDeltaSeconds);
    }

} // namespace Leon::Editor
