#pragma once

#include "Core/Base.hpp"
#include "Core/FTimestep.hpp"
#include "Gameplay/UObject.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "Engine/Components.hpp"
#include "Engine/ENetTypes.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    class AActor;
    class AGameModeBase;
    class AGameStateBase;
    class APlayerController;
    class FWorldRenderer;
    class UNetDriver;

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

        ENetMode GetNetMode() const { return NetMode; }
        void SetNetMode(ENetMode InMode) { NetMode = InMode; }

        void SetNetDriver(UNetDriver* InDriver) { NetDriver = InDriver; }
        UNetDriver* GetNetDriver() const { return NetDriver; }

        // --- Rendering ---
        void OnRender(const FPerspectiveCamera& InCamera);
        FWorldRenderer* GetWorldRenderer();

        /** Project INI renderer defaults applied when the world renderer is first created. */
        void SetProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection);
        bool HasPendingRendererDefaults() const { return bHasPendingRendererDefaults; }
        uint32_t GetPendingShadowMapResolution() const { return PendingShadowMapResolution; }
        bool GetPendingPlanarReflectionEnabled() const { return bPendingPlanarReflection; }

        struct FHitResult {
            bool bBlockingHit = false;
            AActor* Actor = nullptr;
            glm::vec3 Location{0.0f};
            glm::vec3 Normal{0.0f, 1.0f, 0.0f};
            float Distance = 0.0f;
        };

        bool OverlapAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, AActor* InIgnore,
                         FHitResult& OutHit) const;
        bool SweepAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, const glm::vec3& InDelta,
                       AActor* InIgnore, FHitResult& OutHit) const;

    private:
        void DestroyActorImmediate(AActor* InActor);
        void UnbindActorAliases(AActor* InActor);
        void FlushPendingDestroy();

        entt::registry Registry;
        std::vector<TRef<AActor>> Actors;
        std::vector<APlayerController*> PlayerControllers;
        std::vector<AActor*> PendingDestroy;

        AGameModeBase* GameMode = nullptr;
        AGameStateBase* GameState = nullptr;
        ENetMode NetMode = ENetMode::Standalone;
        UNetDriver* NetDriver = nullptr;

        TScope<FWorldRenderer> Renderer;
        bool bBegunPlay = false;
        bool bDeferSpawnedActorBeginPlay = false;
        bool bIsTicking = false;

        bool bHasPendingRendererDefaults = false;
        uint32_t PendingShadowMapResolution = 2048;
        bool bPendingPlanarReflection = true;

        friend class FMapSerializer;
    };

} // namespace Leon
