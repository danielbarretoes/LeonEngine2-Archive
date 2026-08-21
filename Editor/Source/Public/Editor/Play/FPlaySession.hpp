#pragma once

#include "Core/Base.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Editor/Play/FPlaySettings.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/UWorld.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Leon::Editor {

    /**
     * @brief Play In Editor session: owns PlayWorld and drives BeginPlay/Stop.
     */
    class FPlaySession {
    public:
        using FSaveMapFn = std::function<void()>;
        using FLogFn = std::function<void(const std::string& InMessage, bool bInError)>;
        using FSpawnClientFn = std::function<bool(const FPlaySettings& InSettings, int InClientIndex)>;

        void SetSaveMapCallback(FSaveMapFn InFn) { SaveMapFn = std::move(InFn); }
        void SetLogCallback(FLogFn InFn) { LogFn = std::move(InFn); }
        void SetSpawnClientCallback(FSpawnClientFn InFn) { SpawnClientFn = std::move(InFn); }

        [[nodiscard]] bool IsPlaying() const { return bPlaying; }
        [[nodiscard]] UWorld* GetPlayWorld() const { return PlayWorld.get(); }
        [[nodiscard]] const FPlaySettings& GetActiveSettings() const { return ActiveSettings; }

        bool Start(const FPlaySettings& InSettings, const FProjectDescriptor& InProject,
                   const std::string& InMapDiskPath, const std::string& InMapName);
        void Stop();
        void Tick(float InDeltaSeconds);

        /** Request stop on next tick (safe from UI). */
        void RequestStop() { bStopRequested = true; }

        void RegisterChildProcess(void* InHandle) {
            if (InHandle)
                ChildProcessHandles.push_back(InHandle);
        }

    private:
        ENetMode ToEngineNetMode(EPlayNetMode InMode) const;
        bool StartListenIfNeeded();
        bool SpawnExtraClients();

        bool bPlaying = false;
        bool bStopRequested = false;
        FPlaySettings ActiveSettings;
        TRef<UWorld> PlayWorld;
        TRef<UGameInstance> PlayGameInstance;
        FSaveMapFn SaveMapFn;
        FLogFn LogFn;
        FSpawnClientFn SpawnClientFn;
        std::vector<void*> ChildProcessHandles;
    };

} // namespace Leon::Editor
