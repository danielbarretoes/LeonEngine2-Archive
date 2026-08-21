#pragma once

#include "Engine/ENetTypes.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Parameters for starting gameplay on an already-loaded (or empty) UWorld.
     */
    struct FGameplaySessionParams {
        FGameModeConfig GameModeConfig;
        ENetMode NetMode = ENetMode::Standalone;
        bool bSpawnGameMode = true;
        bool bInitWorld = true;
        bool bBeginPlay = true;
        int32_t MaxPlayers = 4;
    };

    /**
     * @brief Shared Start/Stop path used by UEngine boot/travel and Editor PIE.
     */
    class FGameplaySession {
    public:
        /** Resolve GameModeClass from WorldSettings on InWorld, else keep InFallback. */
        static std::string ResolveGameModeClassFromWorld(UWorld& InWorld, const std::string& InFallback);

        static bool Start(UWorld& InWorld, const FGameplaySessionParams& InParams);
        static void Stop(UWorld& InWorld);
    };

} // namespace Leon
