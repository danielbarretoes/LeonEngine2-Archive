#include "Engine/UWorld.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Engine/Components.hpp"
#include "Engine/UNetDriver.hpp"
#include "Assets/UStaticMesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    namespace {

        bool AABBsOverlap(const glm::vec3& AMin, const glm::vec3& AMax, const glm::vec3& BMin,
                          const glm::vec3& BMax) {
            return AMin.x <= BMax.x && AMax.x >= BMin.x && AMin.y <= BMax.y && AMax.y >= BMin.y &&
                   AMin.z <= BMax.z && AMax.z >= BMin.z;
        }

        void TransformAABBCorners(const glm::vec3& InLocalMin, const glm::vec3& InLocalMax, const glm::mat4& InWorld,
                                  glm::vec3& OutMin, glm::vec3& OutMax) {
            OutMin = glm::vec3(std::numeric_limits<float>::max());
            OutMax = glm::vec3(-std::numeric_limits<float>::max());
            const glm::vec3 corners[8] = {
                {InLocalMin.x, InLocalMin.y, InLocalMin.z}, {InLocalMax.x, InLocalMin.y, InLocalMin.z},
                {InLocalMin.x, InLocalMax.y, InLocalMin.z}, {InLocalMax.x, InLocalMax.y, InLocalMin.z},
                {InLocalMin.x, InLocalMin.y, InLocalMax.z}, {InLocalMax.x, InLocalMin.y, InLocalMax.z},
                {InLocalMin.x, InLocalMax.y, InLocalMax.z}, {InLocalMax.x, InLocalMax.y, InLocalMax.z},
            };
            for (const glm::vec3& c : corners) {
                glm::vec3 w = glm::vec3(InWorld * glm::vec4(c, 1.0f));
                OutMin = glm::min(OutMin, w);
                OutMax = glm::max(OutMax, w);
            }
        }

        bool GetActorWorldAABB(AActor& InActor, glm::vec3& OutMin, glm::vec3& OutMax) {
            if (!InActor.HasComponent<FTransformComponent>())
                return false;
            const glm::mat4 world = InActor.GetComponent<FTransformComponent>().GetTransform();

            if (InActor.HasComponent<FBoxCollisionComponent>()) {
                const auto& box = InActor.GetComponent<FBoxCollisionComponent>();
                if (!box.bBlockMovement)
                    return false;
                TransformAABBCorners(box.LocalMin, box.LocalMax, world, OutMin, OutMax);
                return true;
            }

            if (InActor.HasComponent<UStaticMeshComponent>()) {
                const auto& smc = InActor.GetComponent<UStaticMeshComponent>();
                if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                    return false;
                TransformAABBCorners(smc.StaticMesh->GetBoundsMin(), smc.StaticMesh->GetBoundsMax(), world, OutMin,
                                     OutMax);
                return true;
            }

            if (InActor.HasComponent<FMeshComponent>()) {
                const auto& mesh = InActor.GetComponent<FMeshComponent>();
                if (mesh.Mobility != EComponentMobility::Static)
                    return false;
                glm::vec3 localMin(-0.5f);
                glm::vec3 localMax(0.5f);
                if (mesh.MeshType == "Cube") {
                    float h = mesh.MeshSize * 0.5f;
                    localMin = glm::vec3(-h);
                    localMax = glm::vec3(h);
                } else if (mesh.MeshType == "Plane") {
                    localMin = glm::vec3(-mesh.MeshWidth * 0.5f, -0.05f, -mesh.MeshDepth * 0.5f);
                    localMax = glm::vec3(mesh.MeshWidth * 0.5f, 0.05f, mesh.MeshDepth * 0.5f);
                } else if (mesh.MeshType == "Sphere") {
                    localMin = glm::vec3(-mesh.MeshRadius);
                    localMax = glm::vec3(mesh.MeshRadius);
                } else if (mesh.MeshType == "Cylinder" || mesh.MeshType == "Cone") {
                    localMin = glm::vec3(-mesh.MeshRadius, -mesh.MeshHeight * 0.5f, -mesh.MeshRadius);
                    localMax = glm::vec3(mesh.MeshRadius, mesh.MeshHeight * 0.5f, mesh.MeshRadius);
                } else {
                    localMin = glm::vec3(-mesh.MeshWidth * 0.5f, -mesh.MeshHeight * 0.5f, -mesh.MeshDepth * 0.5f);
                    localMax = glm::vec3(mesh.MeshWidth * 0.5f, mesh.MeshHeight * 0.5f, mesh.MeshDepth * 0.5f);
                }
                TransformAABBCorners(localMin, localMax, world, OutMin, OutMax);
                return true;
            }

            return false;
        }

    } // namespace

    TRef<UWorld> UWorld::Create(const std::string& InName) {
        return CreateRef<UWorld>(InName);
    }

    UWorld::UWorld(const std::string& InName) : UObject(InName) {}

    UWorld::~UWorld() {
        Clear();
    }

    void UWorld::Clear() {
        EndPlay();
        PendingDestroy.clear();
        GameMode = nullptr;
        GameState = nullptr;
        PlayerControllers.clear();
        Actors.clear();
        Registry.clear();
        Renderer.reset();
        NetDriver = nullptr;
        bIsTicking = false;
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
            if (actor && !actor->HasBegunPlay() && !actor->IsPendingKill()) {
                actor->ExecuteBeginPlay();
                actor->MarkBegunPlay();
            }
        }
    }

    void UWorld::Tick(FTimestep InTs) {
        float deltaSeconds = InTs.GetSeconds();
        bIsTicking = true;

        if (bBegunPlay) {
            auto tickOnce = [&](AActor* actor) {
                if (!actor || actor->IsPendingKill() || !actor->HasBegunPlay() || !actor->CanEverTick())
                    return;
                actor->ExecuteTick(deltaSeconds);
            };

            // Stable order: GameMode, GameState, PlayerControllers, then remaining actors once.
            tickOnce(GameMode);
            tickOnce(GameState);
            std::vector<APlayerController*> pcs = PlayerControllers;
            for (APlayerController* pc : pcs) {
                tickOnce(pc);
            }

            for (size_t i = 0; i < Actors.size(); ++i) {
                AActor* actor = Actors[i].get();
                if (!actor || actor == GameMode || actor == GameState)
                    continue;
                bool bIsPC = false;
                for (APlayerController* pc : PlayerControllers) {
                    if (pc == actor) {
                        bIsPC = true;
                        break;
                    }
                }
                if (bIsPC)
                    continue;
                tickOnce(actor);
            }
        }

        if (NetDriver)
            NetDriver->Tick(deltaSeconds);

        bIsTicking = false;
        FlushPendingDestroy();
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
        if (!InActor || InActor->IsPendingKill())
            return;

        InActor->MarkPendingKill();
        UnbindActorAliases(InActor);

        if (bBegunPlay && InActor->HasBegunPlay()) {
            InActor->ExecuteEndPlay();
        }

        if (bIsTicking) {
            PendingDestroy.push_back(InActor);
            return;
        }

        DestroyActorImmediate(InActor);
    }

    void UWorld::DestroyActorImmediate(AActor* InActor) {
        if (!InActor)
            return;

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

    void UWorld::UnbindActorAliases(AActor* InActor) {
        if (!InActor)
            return;

        auto* asPC = dynamic_cast<APlayerController*>(InActor);
        auto* asPawn = dynamic_cast<APawn*>(InActor);
        auto* asPS = dynamic_cast<APlayerState*>(InActor);
        auto* asHUD = dynamic_cast<AHUD*>(InActor);
        auto* asPCM = dynamic_cast<APlayerCameraManager*>(InActor);
        auto* asGM = dynamic_cast<AGameModeBase*>(InActor);
        auto* asGS = dynamic_cast<AGameStateBase*>(InActor);

        if (asGM && GameMode == asGM)
            GameMode = nullptr;
        if (asGS && GameState == asGS)
            GameState = nullptr;

        if (asPC) {
            asPC->UnPossess();
            asPC->SetViewTarget(nullptr);
            asPC->SetHUD(nullptr);
            asPC->SetPlayerCameraManager(nullptr);
            asPC->SetPlayerState(nullptr);
            RemovePlayerController(asPC);
        }

        if (asPawn && asPawn->GetController()) {
            asPawn->GetController()->UnPossess();
        }

        if (GameState && asPS)
            GameState->RemovePlayerState(asPS);

        std::vector<APlayerController*> pcs = PlayerControllers;
        for (APlayerController* pc : pcs) {
            if (!pc)
                continue;
            if (pc->GetHUD() == asHUD)
                pc->SetHUD(nullptr);
            if (pc->GetPlayerCameraManager() == asPCM)
                pc->SetPlayerCameraManager(nullptr);
            if (pc->GetPlayerState() == asPS)
                pc->SetPlayerState(nullptr);
            if (pc->GetViewTarget() == InActor)
                pc->SetViewTarget(nullptr);
            if (asPawn && pc->GetPawn() == asPawn)
                pc->UnPossess();
        }
    }

    void UWorld::FlushPendingDestroy() {
        std::vector<AActor*> pending = std::move(PendingDestroy);
        PendingDestroy.clear();
        for (AActor* actor : pending) {
            DestroyActorImmediate(actor);
        }
    }

    void UWorld::RemovePlayerController(APlayerController* InPC) {
        auto it = std::find(PlayerControllers.begin(), PlayerControllers.end(), InPC);
        if (it != PlayerControllers.end())
            PlayerControllers.erase(it);
    }

    AActor* UWorld::FindActorByName(const std::string& InName) {
        for (auto& actor : Actors) {
            if (actor && !actor->IsPendingKill() && actor->GetName() == InName) {
                return actor.get();
            }
        }
        return nullptr;
    }

    AActor* UWorld::FindActorByGuid(const FUUID& InGuid) {
        if (!InGuid.IsValid())
            return nullptr;
        for (auto& actor : Actors) {
            if (actor && !actor->IsPendingKill() && actor->GetActorGuid() == InGuid) {
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

    void UWorld::SetProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection) {
        bHasPendingRendererDefaults = true;
        PendingShadowMapResolution = InShadowMapResolution > 0 ? InShadowMapResolution : 2048;
        bPendingPlanarReflection = bInEnablePlanarReflection;
        if (Renderer) {
            Renderer->ApplyProjectRendererDefaults(PendingShadowMapResolution, bPendingPlanarReflection);
        }
    }

    void UWorld::OnRender(const FPerspectiveCamera& InCamera) {
        GetWorldRenderer()->RenderScene(InCamera);
    }

    bool UWorld::OverlapAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, AActor* InIgnore,
                             FHitResult& OutHit) const {
        OutHit = {};
        for (const auto& actorRef : Actors) {
            if (!actorRef || actorRef.get() == InIgnore || actorRef->IsPendingKill())
                continue;
            glm::vec3 minB, maxB;
            if (!GetActorWorldAABB(*actorRef, minB, maxB))
                continue;
            if (!AABBsOverlap(InWorldMin, InWorldMax, minB, maxB))
                continue;
            OutHit.bBlockingHit = true;
            OutHit.Actor = actorRef.get();
            OutHit.Location = (glm::max(InWorldMin, minB) + glm::min(InWorldMax, maxB)) * 0.5f;
            glm::vec3 overlap = glm::min(InWorldMax, maxB) - glm::max(InWorldMin, minB);
            if (overlap.x <= overlap.y && overlap.x <= overlap.z)
                OutHit.Normal = (InWorldMin.x + InWorldMax.x < minB.x + maxB.x) ? glm::vec3(-1, 0, 0)
                                                                                : glm::vec3(1, 0, 0);
            else if (overlap.y <= overlap.z)
                OutHit.Normal = (InWorldMin.y + InWorldMax.y < minB.y + maxB.y) ? glm::vec3(0, -1, 0)
                                                                                : glm::vec3(0, 1, 0);
            else
                OutHit.Normal = (InWorldMin.z + InWorldMax.z < minB.z + maxB.z) ? glm::vec3(0, 0, -1)
                                                                                : glm::vec3(0, 0, 1);
            return true;
        }
        return false;
    }

    bool UWorld::SweepAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, const glm::vec3& InDelta,
                           AActor* InIgnore, FHitResult& OutHit) const {
        OutHit = {};
        glm::vec3 startMin = InWorldMin;
        glm::vec3 startMax = InWorldMax;
        FHitResult startHit;
        if (OverlapAABB(startMin, startMax, InIgnore, startHit)) {
            OutHit = startHit;
            OutHit.Distance = 0.0f;
            return true;
        }

        glm::vec3 endMin = InWorldMin + InDelta;
        glm::vec3 endMax = InWorldMax + InDelta;
        FHitResult endHit;
        if (!OverlapAABB(endMin, endMax, InIgnore, endHit))
            return false;

        float lo = 0.0f;
        float hi = 1.0f;
        FHitResult best = endHit;
        for (int i = 0; i < 8; ++i) {
            float mid = (lo + hi) * 0.5f;
            glm::vec3 mMin = InWorldMin + InDelta * mid;
            glm::vec3 mMax = InWorldMax + InDelta * mid;
            FHitResult midHit;
            if (OverlapAABB(mMin, mMax, InIgnore, midHit)) {
                hi = mid;
                best = midHit;
                best.Distance = glm::length(InDelta) * mid;
            } else {
                lo = mid;
            }
        }
        OutHit = best;
        OutHit.Location = (InWorldMin + InWorldMax) * 0.5f + InDelta * hi;
        OutHit.bBlockingHit = true;
        return true;
    }

} // namespace Leon
