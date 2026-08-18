#include "Engine/UWorld.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Engine/Components.hpp"
#include "Engine/UNetDriver.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Gameplay/USkeletalMeshComponent.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "AI/UNavigationSystem.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Leon {

    namespace {

        bool AABBsOverlap(const glm::vec3& AMin, const glm::vec3& AMax, const glm::vec3& BMin, const glm::vec3& BMax) {
            return AMin.x <= BMax.x && AMax.x >= BMin.x && AMin.y <= BMax.y && AMax.y >= BMin.y && AMin.z <= BMax.z &&
                   AMax.z >= BMin.z;
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

        bool GetActorWorldAABB(AActor& InActor, ECollisionChannel InQuery, glm::vec3& OutMin, glm::vec3& OutMax,
                               ECollisionChannel& OutChannel) {
            if (!InActor.HasComponent<FTransformComponent>())
                return false;
            const glm::mat4 world = InActor.GetComponent<FTransformComponent>().GetTransform();

            if (InActor.HasComponent<FBoxCollisionComponent>()) {
                const auto& box = InActor.GetComponent<FBoxCollisionComponent>();
                if (!box.bBlockMovement)
                    return false;
                if (!TraceChannelAccepts(InQuery, box.Channel))
                    return false;
                TransformAABBCorners(box.LocalMin, box.LocalMax, world, OutMin, OutMax);
                OutChannel = box.Channel;
                return true;
            }

            const ECollisionChannel implicit = ECollisionChannel::WorldStatic;
            if (!TraceChannelAccepts(InQuery, implicit))
                return false;

            if (InActor.HasComponent<FStaticMeshComponent>()) {
                const auto& smc = InActor.GetComponent<FStaticMeshComponent>();
                if (smc.Mobility != EComponentMobility::Static || !smc.StaticMesh)
                    return false;
                TransformAABBCorners(smc.StaticMesh->GetBoundsMin(), smc.StaticMesh->GetBoundsMax(), world, OutMin,
                                     OutMax);
                OutChannel = implicit;
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
                OutChannel = implicit;
                return true;
            }

            return false;
        }

    } // namespace

    TRef<UWorld> UWorld::Create(const std::string& InName) {
        return CreateRef<UWorld>(InName);
    }

    UWorld::UWorld(const std::string& InName) : UObject(InName) {
        PhysicsScene = FPhysicsModule::CreateScene();
        if (PhysicsScene)
            PhysicsScene->SetWorld(this);
    }

    UWorld::~UWorld() {
        Clear();
    }

    void UWorld::Clear() {
        EndPlay();
        PendingDestroy.clear();
        GameMode = nullptr;
        GameState = nullptr;
        PlayerControllers.clear();
        AIControllers.clear();
        EntityToActor.clear();
        Actors.clear();
        Registry.clear();
        Renderer.reset();
        NetDriver = nullptr;
        PhysicsScene.reset();
        NavigationSystem.reset();
        TimerManager.Clear();
        bIsTicking = false;
        bLightmapsTrusted = true;
    }

    UNavigationSystem* UWorld::GetNavigationSystem() {
        if (!NavigationSystem)
            NavigationSystem = MakeScope<UNavigationSystem>();
        return NavigationSystem.get();
    }

    void UWorld::RebuildNavigation() {
        GetNavigationSystem()->Rebuild(*this);
    }

    void UWorld::InitWorld() {
        if (!PhysicsScene) {
            PhysicsScene = FPhysicsModule::CreateScene();
            if (PhysicsScene)
                PhysicsScene->SetWorld(this);
        }
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
        if (PhysicsScene)
            PhysicsScene->NotifyBeginPlayFinished();
    }

    void UWorld::Tick(FTimestep InTs) {
        float deltaSeconds = std::min(InTs.GetSeconds(), kPhysicsMaxFrameDeltaSeconds);
        if (deltaSeconds < 0.0f)
            deltaSeconds = 0.0f;
        bIsTicking = true;

        if (NetDriver) {
            NetDriver->ConsumeIncomingInput();
            NetDriver->ConsumeIncomingRPCs();
        }

        TimerManager.Tick(deltaSeconds);

        if (bBegunPlay) {
            FFrameProfiler::Working().ActorCount = static_cast<int32_t>(Actors.size());
            auto tickOnce = [&](AActor* actor) {
                if (!actor || actor->IsPendingKill() || !actor->HasBegunPlay() || !actor->CanEverTick())
                    return;
                actor->ExecuteTick(deltaSeconds);
            };

            // Stable order: GameMode, GameState, PlayerControllers, then remaining actors once.
            tickOnce(GameMode);
            tickOnce(GameState);
            {
                FFrameProfiler::FScope controllers(&FFrameProfiler::Working().ControllersMs);
                std::vector<APlayerController*> pcs = PlayerControllers;
                for (APlayerController* pc : pcs) {
                    tickOnce(pc);
                }
            }
            {
                FFrameProfiler::FScope ai(&FFrameProfiler::Working().AIMs);
                std::vector<AAIController*> ais = AIControllers;
                for (AAIController* aiCtrl : ais) {
                    tickOnce(aiCtrl);
                }
            }

            for (size_t i = 0; i < Actors.size(); ++i) {
                AActor* actor = Actors[i].get();
                if (!actor || actor == GameMode || actor == GameState)
                    continue;
                bool bSkip = false;
                for (APlayerController* pc : PlayerControllers) {
                    if (pc == actor) {
                        bSkip = true;
                        break;
                    }
                }
                if (!bSkip) {
                    for (AAIController* ai : AIControllers) {
                        if (ai == actor) {
                            bSkip = true;
                            break;
                        }
                    }
                }
                if (bSkip)
                    continue;
                const bool bCharacter = dynamic_cast<ACharacter*>(actor) != nullptr;
                if (bCharacter) {
                    FFrameProfiler::FScope characters(&FFrameProfiler::Working().CharactersMs);
                    tickOnce(actor);
                } else {
                    tickOnce(actor);
                }
            }
        }

        if (PhysicsScene) {
            FFrameProfiler::FScope physics(&FFrameProfiler::Working().PhysicsMs);
            PhysicsScene->SyncKinematicTransforms();
            PhysicsScene->Tick(deltaSeconds);
            PhysicsScene->SyncDynamicTransforms();
        }

        if (bBegunPlay) {
            for (const auto& actorRef : Actors) {
                AActor* actor = actorRef.get();
                if (!actor || actor->IsPendingKill())
                    continue;
                for (const auto& compRef : actor->GetActorComponents()) {
                    auto* skel = dynamic_cast<USkeletalMeshComponent*>(compRef.get());
                    if (skel && skel->IsRagdoll())
                        skel->ApplyRagdollPoseFromBodies();
                }
            }
        }

        if (PhysicsScene)
            PhysicsScene->DrainContacts();

        if (bBegunPlay) {
            FFrameProfiler::FScope overlaps(&FFrameProfiler::Working().OverlapsMs);
            UpdateComponentOverlaps();
        }

        if (NetDriver) {
            FFrameProfiler::FScope net(&FFrameProfiler::Working().NetworkMs);
            NetDriver->Tick(deltaSeconds);
        }

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
        if (handle != entt::null)
            EntityToActor.erase(static_cast<uint32_t>(handle));
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

        if (auto* asAI = dynamic_cast<AAIController*>(InActor)) {
            asAI->UnPossess();
            asAI->SetPlayerState(nullptr);
            auto it = std::find(AIControllers.begin(), AIControllers.end(), asAI);
            if (it != AIControllers.end())
                AIControllers.erase(it);
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

    AActor* UWorld::FindActorByEntity(entt::entity InEntity) const {
        if (InEntity == entt::null)
            return nullptr;
        auto it = EntityToActor.find(static_cast<uint32_t>(InEntity));
        if (it == EntityToActor.end() || !it->second || it->second->IsPendingKill())
            return nullptr;
        return it->second;
    }

    void UWorld::AddPlayerController(APlayerController* InPC) {
        if (InPC && std::find(PlayerControllers.begin(), PlayerControllers.end(), InPC) == PlayerControllers.end()) {
            PlayerControllers.push_back(InPC);
        }
    }

    void UWorld::AddAIController(AAIController* InAI) {
        if (InAI && std::find(AIControllers.begin(), AIControllers.end(), InAI) == AIControllers.end()) {
            AIControllers.push_back(InAI);
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

    void UWorld::SetProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection,
                                            uint32_t InCascadeCount, float InShadowDistance,
                                            EPlanarReflectionQuality InPlanarQuality, float InPlanarResolutionScale) {
        bHasPendingRendererDefaults = true;
        PendingShadowMapResolution = InShadowMapResolution > 0 ? InShadowMapResolution : 2048;
        bPendingPlanarReflection = bInEnablePlanarReflection;
        PendingPlanarQuality = InPlanarQuality;
        PendingPlanarResolutionScale = InPlanarResolutionScale > 0.0f
                                           ? ClampPlanarReflectionResolutionScale(InPlanarResolutionScale)
                                           : PlanarReflectionScaleFor(InPlanarQuality);
        if (InCascadeCount > 0)
            PendingCascadeCount = std::min(InCascadeCount, 4u);
        if (InShadowDistance > 0.0f)
            PendingShadowDistance = InShadowDistance;
        if (Renderer) {
            Renderer->ApplyProjectRendererDefaults(PendingShadowMapResolution, bPendingPlanarReflection,
                                                   PendingPlanarQuality, PendingPlanarResolutionScale);
            Renderer->GetShadowSettings().CascadeCount = PendingCascadeCount;
            Renderer->GetShadowSettings().ShadowDistance = PendingShadowDistance;
        }
    }

    void UWorld::SetProjectSSAODefaults(bool bInEnabled, float InRadius, float InIntensity, float InBias) {
        bPendingSSAOEnabled = bInEnabled;
        PendingSSAORadius = InRadius > 0.0f ? InRadius : 0.5f;
        PendingSSAOIntensity = InIntensity >= 0.0f ? InIntensity : 1.0f;
        PendingSSAOBias = InBias >= 0.0f ? InBias : 0.025f;
        if (Renderer) {
            auto& pp = Renderer->GetPostProcessSettings();
            pp.bSSAOEnabled = bPendingSSAOEnabled;
            pp.SSAORadius = PendingSSAORadius;
            pp.SSAOIntensity = PendingSSAOIntensity;
            pp.SSAOBias = PendingSSAOBias;
        }
    }

    void UWorld::OnRender(const FPerspectiveCamera& InCamera) {
        GetWorldRenderer()->RenderScene(InCamera);
    }

    bool UWorld::OverlapAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, AActor* InIgnore,
                             FHitResult& OutHit) const {
        glm::vec3 center = (InWorldMin + InWorldMax) * 0.5f;
        glm::vec3 half = (InWorldMax - InWorldMin) * 0.5f;
        std::vector<FHitResult> hits;
        if (OverlapMultiByChannel(center, half, ECollisionChannel::Visibility, InIgnore, hits) <= 0) {
            OutHit = {};
            return false;
        }
        OutHit = hits.front();
        return true;
    }

    bool UWorld::SweepAABB(const glm::vec3& InWorldMin, const glm::vec3& InWorldMax, const glm::vec3& InDelta,
                           AActor* InIgnore, FHitResult& OutHit) const {
        glm::vec3 start = (InWorldMin + InWorldMax) * 0.5f;
        glm::vec3 half = (InWorldMax - InWorldMin) * 0.5f;
        float radius = std::max(half.x, std::max(half.y, half.z));
        return SweepSingleByChannel(start, start + InDelta, radius, ECollisionChannel::WorldStatic, InIgnore, OutHit);
    }

    bool UWorld::LineTrace(const glm::vec3& InStart, const glm::vec3& InEnd, AActor* InIgnore,
                           FHitResult& OutHit) const {
        return LineTraceSingleByChannel(InStart, InEnd, ECollisionChannel::Visibility, InIgnore, OutHit);
    }

    bool UWorld::LineTraceByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                    AActor* InIgnore, FHitResult& OutHit) const {
        return LineTraceSingleByChannel(InStart, InEnd, InChannel, InIgnore, OutHit);
    }

    bool UWorld::LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                          AActor* InIgnore, FHitResult& OutHit) const {
        OutHit = {};
        bool bHit = false;
        if (PhysicsScene)
            bHit = PhysicsScene->LineTraceSingleByChannel(InStart, InEnd, InChannel, InIgnore, OutHit);
        if (FDebugRenderer::IsTraceCaptureEnabled()) {
            FDebugRenderer::RecordLineTrace(InStart, InEnd, bHit && OutHit.bBlockingHit, OutHit.Location, OutHit.Normal,
                                            static_cast<uint8_t>(InChannel));
        }
        return bHit;
    }

    int32_t UWorld::LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                            ECollisionChannel InChannel, AActor* InIgnore,
                                            std::vector<FHitResult>& OutHits) const {
        if (!PhysicsScene) {
            OutHits.clear();
            return 0;
        }
        return PhysicsScene->LineTraceMultiByChannel(InStart, InEnd, InChannel, InIgnore, OutHits);
    }

    bool UWorld::SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                      ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const {
        OutHit = {};
        if (!PhysicsScene)
            return false;
        return PhysicsScene->SweepSingleByChannel(InStart, InEnd, InRadius, InChannel, InIgnore, OutHit);
    }

    int32_t UWorld::SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                        ECollisionChannel InChannel, AActor* InIgnore,
                                        std::vector<FHitResult>& OutHits) const {
        if (!PhysicsScene) {
            OutHits.clear();
            return 0;
        }
        return PhysicsScene->SweepMultiByChannel(InStart, InEnd, InRadius, InChannel, InIgnore, OutHits);
    }

    bool UWorld::OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                         ECollisionChannel InChannel, AActor* InIgnore) const {
        return PhysicsScene && PhysicsScene->OverlapAnyTestByChannel(InPos, InHalfExtent, InChannel, InIgnore);
    }

    int32_t UWorld::OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                          ECollisionChannel InChannel, AActor* InIgnore,
                                          std::vector<FHitResult>& OutHits) const {
        if (!PhysicsScene) {
            OutHits.clear();
            return 0;
        }
        return PhysicsScene->OverlapMultiByChannel(InPos, InHalfExtent, InChannel, InIgnore, OutHits);
    }

    void UWorld::UpdateComponentOverlaps() {
        std::vector<UPrimitiveComponent*> generators;
        std::vector<UPrimitiveComponent*> candidates;
        generators.reserve(32);
        candidates.reserve(64);

        for (const auto& actorRef : Actors) {
            AActor* actor = actorRef.get();
            if (!actor || actor->IsPendingKill() || !actor->HasBegunPlay())
                continue;
            for (const auto& compRef : actor->GetActorComponents()) {
                UPrimitiveComponent* prim = compRef ? compRef->AsPrimitiveComponent() : nullptr;
                if (!prim || prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
                    continue;
                candidates.push_back(prim);
                if (prim->GetGenerateOverlapEvents())
                    generators.push_back(prim);
            }
        }

        for (UPrimitiveComponent* gen : generators)
            gen->UpdateOverlaps(candidates);
    }

} // namespace Leon
