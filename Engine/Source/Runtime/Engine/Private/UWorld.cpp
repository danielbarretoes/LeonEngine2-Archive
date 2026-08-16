#include "Engine/UWorld.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Engine/Components.hpp"

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
        GameMode = nullptr;
        GameState = nullptr;
        PlayerControllers.clear();
        Actors.clear();
        Registry.clear();
        Renderer.reset();
    }

    void UWorld::InitWorld() {
        if (GameMode) {
            GameMode->InitGame();
        }
    }

    void UWorld::BeginPlay() {
        if (bBegunPlay)
            return;
        bBegunPlay = true;

        LE_CORE_INFO("UWorld: Beginning play across all actors (Count: {0})...", Actors.size());

        // Flow: StartPlay/Login spawns & wires PC/HUD/Pawn first; BeginPlay runs after wiring.
        bDeferSpawnedActorBeginPlay = true;
        if (GameMode) {
            GameMode->StartPlay();
        }
        bDeferSpawnedActorBeginPlay = false;

        for (auto& actor : Actors) {
            if (actor && !actor->HasBegunPlay()) {
                actor->ExecuteBeginPlay();
                actor->MarkBegunPlay();
            }
        }
    }

    void UWorld::Tick(FTimestep InTs) {
        float deltaSeconds = InTs.GetSeconds();

        // 1. Tick GameMode
        if (GameMode && GameMode->CanEverTick()) {
            GameMode->Tick(deltaSeconds);
        }

        // 2. Tick GameState
        if (GameState && GameState->CanEverTick()) {
            GameState->Tick(deltaSeconds);
        }

        // 3. Tick PlayerControllers
        for (auto* pc : PlayerControllers) {
            if (pc && pc->CanEverTick()) {
                pc->Tick(deltaSeconds);
            }
        }

        // 4. Tick all spawned actors (+ lite UActorComponents)
        for (size_t i = 0; i < Actors.size(); ++i) {
            auto& actor = Actors[i];
            if (actor && actor->CanEverTick() && actor->HasBegunPlay()) {
                actor->ExecuteTick(deltaSeconds);
            }
        }
    }

    void UWorld::EndPlay() {
        if (!bBegunPlay)
            return;

        LE_CORE_INFO("UWorld: Ending play across all actors...");
        for (auto& actor : Actors) {
            if (actor && actor->HasBegunPlay()) {
                actor->ExecuteEndPlay();
            }
        }

        bBegunPlay = false;
    }

    AActor* UWorld::SpawnActor(const std::string& InName) {
        return SpawnActor<AActor>(InName);
    }

    void UWorld::DestroyActor(AActor* InActor) {
        if (!InActor)
            return;

        if (bBegunPlay && InActor->HasBegunPlay()) {
            InActor->ExecuteEndPlay();
        }

        entt::entity handle = InActor->GetEntityHandle();
        if (handle != entt::null && Registry.valid(handle)) {
            Registry.destroy(handle);
        }

        auto it = std::find_if(Actors.begin(), Actors.end(),
                               [InActor](const TRef<AActor>& item) { return item.get() == InActor; });
        if (it != Actors.end()) {
            Actors.erase(it);
        }
    }

    AActor* UWorld::FindActorByName(const std::string& InName) {
        for (auto& actor : Actors) {
            if (actor && actor->GetName() == InName) {
                return actor.get();
            }
        }
        return nullptr;
    }

    void UWorld::AddPlayerController(APlayerController* InPC) {
        if (InPC &&
            std::find(PlayerControllers.begin(), PlayerControllers.end(), InPC) == PlayerControllers.end()) {
            PlayerControllers.push_back(InPC);
        }
    }

    APlayerController* UWorld::GetFirstPlayerController() const {
        return PlayerControllers.empty() ? nullptr : PlayerControllers.front();
    }

    FWorldRenderer* UWorld::GetWorldRenderer() {
        if (!Renderer) {
            Renderer = CreateScope<FWorldRenderer>(this);
        }
        return Renderer.get();
    }

    void UWorld::OnRender(const FPerspectiveCamera& InCamera) {
        GetWorldRenderer()->RenderScene(InCamera);
    }

} // namespace Leon
