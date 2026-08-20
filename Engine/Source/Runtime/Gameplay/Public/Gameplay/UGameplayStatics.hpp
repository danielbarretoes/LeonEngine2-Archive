#pragma once

#include "Core/Base.hpp"
#include "Engine/FParticleTypes.hpp"
#include "Gameplay/FDamageInfo.hpp"
#include "Physics/FHitResult.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class UWorld;
    class APlayerController;
    class AGameModeBase;
    class AGameStateBase;
    class APawn;
    class AActor;
    class UParticleComponent;

    /**
     * @brief Unreal Engine aligned static library for gameplay functions.
     */
    class UGameplayStatics {
    public:
        /**
         * @brief Requests loading of a new level map (e.g. "/Game/Maps/MyMap").
         */
        static void OpenLevel(UWorld* InWorldContext, const std::string& InLevelName);

        /**
         * @brief Displays an on-screen debug message in the viewport (not FLog).
         * @param InKey When >= 0, replaces any existing message with the same key.
         * @param InPosition Screen-space pixels; (-1,-1) auto-stacks from the top-left.
         */
        static void PrintString(const std::string& InMessage, float InDuration = 2.0f,
                                const glm::vec4& InColor = glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), int64_t InKey = -1,
                                const glm::vec2& InPosition = glm::vec2(-1.0f, -1.0f));

        static APlayerController* GetPlayerController(UWorld* InWorldContext, int32_t InPlayerIndex = 0);
        static AGameModeBase* GetGameMode(UWorld* InWorldContext);
        static AGameStateBase* GetGameState(UWorld* InWorldContext);
        static APawn* GetPlayerPawn(UWorld* InWorldContext, int32_t InPlayerIndex = 0);

        static bool SpawnEmitterAtLocation(UWorld* InWorld, const FParticleEmitterSettings& InSettings,
                                           const glm::vec3& InLocation);

        /** Fire-and-forget 2D one-shot from a virtual/physical sound path. */
        static void PlaySound2D(const std::string& InSoundPath, float InVolume = 1.0f);

        /** Looping menu/ambient music; stops any previous music track. */
        static void PlayMusic2D(const std::string& InSoundPath, float InVolume = 1.0f);
        static void StopMusic();

        /** Fire-and-forget 3D one-shot with distance attenuation. */
        static void PlaySoundAtLocation(const std::string& InSoundPath, const glm::vec3& InLocation,
                                        float InVolume = 1.0f, float InAttenuationRadius = 2500.0f);

        /**
         * Authority-only point damage into UHealthComponent on DamagedActor.
         * Notifies AGameModeBase::NotifyActorDamaged / NotifyActorKilled when present.
         * @return true if health was modified.
         */
        static bool ApplyPointDamage(UWorld* InWorld, AActor* DamagedActor, float BaseDamage,
                                     const glm::vec3& HitFromDirection, const FHitResult& HitInfo,
                                     AActor* DamageInstigator, AActor* DamageCauser);

        /**
         * Authority-only radial damage to all actors with UHealthComponent in radius (linear falloff).
         * @return number of actors that took damage.
         */
        static int32_t ApplyRadialDamage(UWorld* InWorld, float BaseDamage, const glm::vec3& Origin, float DamageRadius,
                                         AActor* DamageInstigator, AActor* DamageCauser, float MinimumDamage = 0.0f,
                                         AActor* IgnoreActor = nullptr);
    };

    /**
     * @brief Global helper for visual on-screen debug printing (Unreal PrintString equivalent).
     */
    inline void PrintString(const std::string& InMessage, float InDuration = 2.0f,
                            const glm::vec4& InColor = glm::vec4(0.0f, 0.85f, 1.0f, 1.0f), int64_t InKey = -1,
                            const glm::vec2& InPosition = glm::vec2(-1.0f, -1.0f)) {
        UGameplayStatics::PrintString(InMessage, InDuration, InColor, InKey, InPosition);
    }

} // namespace Leon
