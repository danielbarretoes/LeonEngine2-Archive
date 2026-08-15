#pragma once

#include "core/Base.hpp"
#include "core/Timestep.hpp"
#include "gameplay/UObject.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "world/Components.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    class AActor;
    class AGameModeBase;
    class AGameStateBase;
    class APlayerController;
    class FSceneRenderer;

    /**
     * @brief Unreal Engine aligned UWorld runtime container representing loaded map instances.
     *
     * Owns the actor list, ECS registry, GameMode, GameState, PlayerControllers, and lazy-initialized FSceneRenderer.
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
        bool HasBegunPlay() const { return m_bBegunPlay; }

        // --- Actor Management ---
        AActor* SpawnActor(const std::string& InName = "Actor");

        template <typename T, typename... TArgs>
        T* SpawnActor(const std::string& InName = "Actor", TArgs&&... InArgs) {
            entt::entity handle = m_Registry.create();
            auto actor = std::make_shared<T>(handle, this, InName.empty() ? "Actor" : InName,
                                             std::forward<TArgs>(InArgs)...);
            m_Actors.push_back(actor);

            actor->SetName(InName.empty() ? "Actor" : InName);
            if (!actor->template HasComponent<FTransformComponent>()) {
                actor->template AddComponent<FTransformComponent>();
            }

            actor->PostInitializeComponents();

            if (m_bBegunPlay && !actor->HasBegunPlay()) {
                actor->BeginPlay();
                actor->MarkBegunPlay();
            }

            return actor.get();
        }

        void DestroyActor(AActor* InActor);
        AActor* FindActorByName(const std::string& InName);
        const std::vector<TRef<AActor>>& GetAllActors() const { return m_Actors; }

        // --- ECS Internal Access ---
        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        // --- Gameplay Framework Accessors ---
        void SetGameMode(AGameModeBase* InGameMode) { m_GameMode = InGameMode; }
        AGameModeBase* GetGameMode() const { return m_GameMode; }

        void SetGameState(AGameStateBase* InGameState) { m_GameState = InGameState; }
        AGameStateBase* GetGameState() const { return m_GameState; }

        void AddPlayerController(APlayerController* InPC);
        APlayerController* GetFirstPlayerController() const;
        const std::vector<APlayerController*>& GetPlayerControllers() const { return m_PlayerControllers; }

        // --- Rendering ---
        void OnRender(const FPerspectiveCamera& InCamera);
        FSceneRenderer* GetSceneRenderer();

    private:
        entt::registry m_Registry;
        std::vector<TRef<AActor>> m_Actors;
        std::vector<APlayerController*> m_PlayerControllers;

        AGameModeBase* m_GameMode = nullptr;
        AGameStateBase* m_GameState = nullptr;

        TScope<FSceneRenderer> m_Renderer;
        bool m_bBegunPlay = false;

        friend class MapSerializer;
    };

} // namespace Leon
