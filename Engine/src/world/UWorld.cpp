#include "world/UWorld.hpp"
#include "core/Log.hpp"
#include "gameplay/AActor.hpp"
#include "gameplay/AGameModeBase.hpp"
#include "gameplay/AGameStateBase.hpp"
#include "gameplay/APlayerController.hpp"
#include "renderer/SceneRenderer.hpp"
#include "world/Components.hpp"

#include <algorithm>

namespace Leon {

    TRef<UWorld> UWorld::Create(const std::string& InName) {
        return CreateRef<UWorld>(InName);
    }

    UWorld::UWorld(const std::string& InName) : UObject(InName) {}

    UWorld::~UWorld() {
        Clear();
    }

    void UWorld::Clear() {
        EndPlay();
        m_GameMode = nullptr;
        m_GameState = nullptr;
        m_PlayerControllers.clear();
        m_Actors.clear();
        m_Registry.clear();
    }

    void UWorld::InitWorld() {
        if (m_GameMode) {
            m_GameMode->InitGame();
        }
    }

    void UWorld::BeginPlay() {
        if (m_bBegunPlay) return;
        m_bBegunPlay = true;

        LE_CORE_INFO("UWorld: Beginning play across all actors (Count: {0})...", m_Actors.size());

        if (m_GameMode) {
            m_GameMode->StartPlay();
        }

        // Trigger BeginPlay on all actors
        for (auto& actor : m_Actors) {
            if (actor && !actor->HasBegunPlay()) {
                actor->BeginPlay();
                actor->MarkBegunPlay();
            }
        }
    }

    void UWorld::Tick(FTimestep InTs) {
        float deltaSeconds = InTs.GetSeconds();

        // 1. Tick GameMode
        if (m_GameMode && m_GameMode->CanEverTick()) {
            m_GameMode->Tick(deltaSeconds);
        }

        // 2. Tick GameState
        if (m_GameState && m_GameState->CanEverTick()) {
            m_GameState->Tick(deltaSeconds);
        }

        // 3. Tick PlayerControllers
        for (auto* pc : m_PlayerControllers) {
            if (pc && pc->CanEverTick()) {
                pc->Tick(deltaSeconds);
            }
        }

        // 4. Tick all spawned actors
        for (size_t i = 0; i < m_Actors.size(); ++i) {
            auto& actor = m_Actors[i];
            if (actor && actor->CanEverTick() && actor->HasBegunPlay()) {
                actor->Tick(deltaSeconds);
            }
        }
    }

    void UWorld::EndPlay() {
        if (!m_bBegunPlay) return;

        LE_CORE_INFO("UWorld: Ending play across all actors...");
        for (auto& actor : m_Actors) {
            if (actor && actor->HasBegunPlay()) {
                actor->EndPlay();
            }
        }

        m_bBegunPlay = false;
    }

    AActor* UWorld::SpawnActor(const std::string& InName) {
        return SpawnActor<AActor>(InName);
    }

    void UWorld::DestroyActor(AActor* InActor) {
        if (!InActor) return;

        // Notify EndPlay if running
        if (m_bBegunPlay && InActor->HasBegunPlay()) {
            InActor->EndPlay();
        }

        entt::entity handle = InActor->GetEntityHandle();
        if (handle != entt::null && m_Registry.valid(handle)) {
            m_Registry.destroy(handle);
        }

        auto it = std::find_if(m_Actors.begin(), m_Actors.end(),
                               [InActor](const TRef<AActor>& item) { return item.get() == InActor; });
        if (it != m_Actors.end()) {
            m_Actors.erase(it);
        }
    }

    AActor* UWorld::FindActorByName(const std::string& InName) {
        for (auto& actor : m_Actors) {
            if (actor && actor->GetName() == InName) {
                return actor.get();
            }
        }
        return nullptr;
    }

    void UWorld::AddPlayerController(APlayerController* InPC) {
        if (InPC && std::find(m_PlayerControllers.begin(), m_PlayerControllers.end(), InPC) == m_PlayerControllers.end()) {
            m_PlayerControllers.push_back(InPC);
        }
    }

    APlayerController* UWorld::GetFirstPlayerController() const {
        return m_PlayerControllers.empty() ? nullptr : m_PlayerControllers.front();
    }

    FSceneRenderer* UWorld::GetSceneRenderer() {
        if (!m_Renderer) {
            m_Renderer = CreateScope<FSceneRenderer>(this);
        }
        return m_Renderer.get();
    }

    void UWorld::OnRender(const FPerspectiveCamera& InCamera) {
        GetSceneRenderer()->RenderScene(InCamera);
    }

} // namespace Leon
