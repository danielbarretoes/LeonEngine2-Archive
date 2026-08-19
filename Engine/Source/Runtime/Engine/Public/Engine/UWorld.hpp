#pragma once

#include "Core/Base.hpp"
#include "Core/FTimestep.hpp"
#include "Gameplay/UObject.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "Engine/Components.hpp"
#include "Engine/ENetTypes.hpp"
#include "Engine/FTimerManager.hpp"
#include "Physics/FHitResult.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    class AActor;
    class AGameModeBase;
    class AGameStateBase;
    class APlayerController;
    class AAIController;
    class FWorldRenderer;
    class UNetDriver;
    class UNavigationSystem;

    /**
     * @brief Unreal Engine aligned UWorld runtime container representing loaded map instances.
     *
     * Owns the actor list, ECS registry, GameMode, GameState, PlayerControllers, and lazy-initialized FWorldRenderer.
     */
    class UWorld : public UObject {
    public:
        static TRef<UWorld> Create(const std::string& InName = "World");

        UWorld(const std::string& InName = "World");
        ~UWorld() override;

        // --- Lifecycle ---
        void InitWorld();
        void Clear();
        void BeginPlay();
        void Tick(FTimestep InTs);
        void EndPlay();
        bool HasBegunPlay() const { return bBegunPlay; }

        // --- Actor Management ---
        AActor* SpawnActor(const std::string& InName = "Actor");

        template <typename T, typename... TArgs> T* SpawnActor(const std::string& InName = "Actor", TArgs&&... InArgs) {
            entt::entity handle = Registry.create();
            auto actor =
                std::make_shared<T>(handle, this, InName.empty() ? "Actor" : InName, std::forward<TArgs>(InArgs)...);
            Actors.push_back(actor);
            EntityToActor[static_cast<uint32_t>(handle)] = actor.get();

            actor->SetName(InName.empty() ? "Actor" : InName);
            if (!actor->template HasComponent<FTransformComponent>()) {
                actor->template AddComponent<FTransformComponent>();
            }

            actor->PostInitializeComponents();

            // During StartPlay/Login, actors are wired (HUD↔PC, Possess) before BeginPlay runs.
            if (bBegunPlay && !bDeferSpawnedActorBeginPlay && !actor->HasBegunPlay() && !actor->IsPendingKill()) {
                actor->ExecuteBeginPlay();
                actor->MarkBegunPlay();
            }

            return actor.get();
        }

        void DestroyActor(AActor* InActor);
        void RemovePlayerController(APlayerController* InPC);
        AActor* FindActorByName(const std::string& InName);
        AActor* FindActorByGuid(const FUUID& InGuid);
        AActor* FindActorByEntity(entt::entity InEntity) const;
        const std::vector<TRef<AActor>>& GetAllActors() const { return Actors; }

        /**
         * @brief When true, SpawnActor will not auto-call BeginPlay (used during StartPlay/Login wiring).
         */
        bool IsDeferringSpawnedActorBeginPlay() const { return bDeferSpawnedActorBeginPlay; }

        // --- ECS Internal Access ---
        entt::registry& GetRegistry() { return Registry; }
        const entt::registry& GetRegistry() const { return Registry; }

        // --- Gameplay Framework Accessors ---
        void SetGameMode(AGameModeBase* InGameMode) { GameMode = InGameMode; }
        AGameModeBase* GetGameMode() const { return GameMode; }

        void SetGameState(AGameStateBase* InGameState) { GameState = InGameState; }
        AGameStateBase* GetGameState() const { return GameState; }

        void AddPlayerController(APlayerController* InPC);
        APlayerController* GetFirstPlayerController() const;
        const std::vector<APlayerController*>& GetPlayerControllers() const { return PlayerControllers; }

        void AddAIController(AAIController* InAI);
        const std::vector<AAIController*>& GetAIControllers() const { return AIControllers; }

        IPhysicsScene* GetPhysicsScene() const { return PhysicsScene.get(); }
        UNavigationSystem* GetNavigationSystem();
        void RebuildNavigation();

        using FHitResult = Leon::FHitResult;

        ENetMode GetNetMode() const { return NetMode; }
        void SetNetMode(ENetMode InMode) { NetMode = InMode; }

        void SetNetDriver(UNetDriver* InDriver) { NetDriver = InDriver; }
        UNetDriver* GetNetDriver() const { return NetDriver; }

        FTimerManager& GetTimerManager() { return TimerManager; }
        const FTimerManager& GetTimerManager() const { return TimerManager; }

        // --- Rendering ---
        void OnRender(const FPerspectiveCamera& InCamera);
        FWorldRenderer* GetWorldRenderer();

        bool AreLightmapsTrusted() const { return bLightmapsTrusted; }
        void SetLightmapsTrusted(bool bTrusted) { bLightmapsTrusted = bTrusted; }

        /** Project INI renderer defaults applied when the world renderer is first created. */
        void SetProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection,
                                        uint32_t InCascadeCount = 0, float InShadowDistance = 0.0f,
                                        EPlanarReflectionQuality InPlanarQuality = EPlanarReflectionQuality::Epic,
                                        float InPlanarResolutionScale = 0.0f);
        void SetProjectSSAODefaults(bool bInEnabled, float InRadius, float InIntensity, float InBias);
        void SetProjectPostProcessToggles(bool bInBloomEnabled, bool bInFXAAEnabled);
        void SetProjectShadowFilter(EShadowFilterMode InFilter);
        void SetProjectOmniShadowDefaults(uint32_t InSpotResolution, uint32_t InPointShadowResolution,
                                          uint32_t InMaxShadowedPointLights);
        bool GetPendingSSAOEnabled() const { return bPendingSSAOEnabled; }
        float GetPendingSSAORadius() const { return PendingSSAORadius; }
        float GetPendingSSAOIntensity() const { return PendingSSAOIntensity; }
        float GetPendingSSAOBias() const { return PendingSSAOBias; }
        bool GetPendingBloomEnabled() const { return bPendingBloomEnabled; }
        bool GetPendingFXAAEnabled() const { return bPendingFXAAEnabled; }
        EShadowFilterMode GetPendingShadowFilter() const { return PendingShadowFilter; }

        bool HasPendingRendererDefaults() const { return bHasPendingRendererDefaults; }
        uint32_t GetPendingShadowMapResolution() const { return PendingShadowMapResolution; }
        bool GetPendingPlanarReflectionEnabled() const { return bPendingPlanarReflection; }
        uint32_t GetPendingCascadeCount() const { return PendingCascadeCount; }
        float GetPendingShadowDistance() const { return PendingShadowDistance; }
        EPlanarReflectionQuality GetPendingPlanarReflectionQuality() const { return PendingPlanarQuality; }
        float GetPendingPlanarReflectionResolutionScale() const { return PendingPlanarResolutionScale; }
        uint32_t GetPendingSpotResolution() const { return PendingSpotResolution; }
        uint32_t GetPendingPointShadowResolution() const { return PendingPointShadowResolution; }
        uint32_t GetPendingMaxShadowedPointLights() const { return PendingMaxShadowedPointLights; }

        bool OverlapAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, AActor* InIgnore,
                         FHitResult& OutHit) const;
        bool SweepAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, const glm::vec3& InDelta,
                       AActor* InIgnore, FHitResult& OutHit) const;
        bool LineTrace(const glm::vec3& InStart, const glm::vec3& InEnd, AActor* InIgnore, FHitResult& OutHit) const;
        bool LineTraceByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                AActor* InIgnore, FHitResult& OutHit) const;
        bool LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                      AActor* InIgnore, FHitResult& OutHit) const;
        int32_t LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                        AActor* InIgnore, std::vector<FHitResult>& OutHits) const;
        bool SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                  ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const;
        int32_t SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                    ECollisionChannel InChannel, AActor* InIgnore,
                                    std::vector<FHitResult>& OutHits) const;
        bool SweepCapsuleSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                         float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                         FHitResult& OutHit) const;
        int32_t SweepCapsuleMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                           float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                           std::vector<FHitResult>& OutHits) const;
        bool OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent, ECollisionChannel InChannel,
                                     AActor* InIgnore) const;
        int32_t OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                      ECollisionChannel InChannel, AActor* InIgnore,
                                      std::vector<FHitResult>& OutHits) const;

        /** Diff overlap generators vs all primitives; fires Begin/End overlap delegates. */
        void UpdateComponentOverlaps();

    private:
        void DestroyActorImmediate(AActor* InActor);
        void UnbindActorAliases(AActor* InActor);
        void FlushPendingDestroy();

        entt::registry Registry;
        std::vector<TRef<AActor>> Actors;
        std::unordered_map<uint32_t, AActor*> EntityToActor;
        std::vector<APlayerController*> PlayerControllers;
        std::vector<AAIController*> AIControllers;
        std::vector<AActor*> PendingDestroy;

        AGameModeBase* GameMode = nullptr;
        AGameStateBase* GameState = nullptr;
        ENetMode NetMode = ENetMode::Standalone;
        UNetDriver* NetDriver = nullptr;
        FTimerManager TimerManager;
        TRef<IPhysicsScene> PhysicsScene;
        TScope<UNavigationSystem> NavigationSystem;

        TScope<FWorldRenderer> Renderer;
        bool bBegunPlay = false;
        bool bDeferSpawnedActorBeginPlay = false;
        bool bIsTicking = false;
        bool bLightmapsTrusted = true;

        bool bHasPendingRendererDefaults = false;
        uint32_t PendingShadowMapResolution = 2048;
        bool bPendingPlanarReflection = true;
        uint32_t PendingCascadeCount = 4;
        float PendingShadowDistance = 100.0f;
        EPlanarReflectionQuality PendingPlanarQuality = EPlanarReflectionQuality::Epic;
        float PendingPlanarResolutionScale = 1.0f;
        bool bPendingSSAOEnabled = true;
        float PendingSSAORadius = 0.5f;
        float PendingSSAOIntensity = 1.0f;
        float PendingSSAOBias = 0.025f;
        bool bPendingBloomEnabled = true;
        bool bPendingFXAAEnabled = true;
        EShadowFilterMode PendingShadowFilter = EShadowFilterMode::PCF3x3;
        uint32_t PendingSpotResolution = 1024;
        uint32_t PendingPointShadowResolution = 512;
        uint32_t PendingMaxShadowedPointLights = 4;

        friend class FMapSerializer;
    };

} // namespace Leon
