#pragma once

#include "Core/Base.hpp"
#include "Core/FLog.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Gameplay/UActorComponent.hpp"
#include "Gameplay/UObject.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"

#include <entt/entt.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    class UWorld;
    class USceneComponent;

    /**
     * @brief Base class for any object placed or spawned within a UWorld.
     *
     * Lifecycle: PostInitializeComponents -> BeginPlay -> Tick -> EndPlay -> Destroy
     * Owns optional lite UActorComponent list (logic). EnTT POD comps via AddComponent<T>().
     *
     * Transform contract (Unreal-lite): FTransformComponent is the actor world pose and the
     * RootComponent's world pose. Root relative transform stays identity. Get/SetActorLocation
     * read/write EnTT only; attached USceneComponents resolve through the attach hierarchy.
     */
    class AActor : public UObject {
    public:
        AActor() = default;
        AActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Actor");
        ~AActor() override = default;

        virtual void PostInitializeComponents() {}
        virtual void BeginPlay() {}
        virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
        virtual void EndPlay() {}
        virtual void Destroy();

        /** Called by UWorld — runs actor BeginPlay then owned UActorComponents. */
        void ExecuteBeginPlay();
        void ExecuteTick(float DeltaSeconds);
        void ExecuteEndPlay();

        UWorld* GetWorld() const { return World; }
        entt::entity GetEntityHandle() const { return EntityHandle; }

        const FUUID& GetActorGuid() const { return ActorGuid; }
        void SetActorGuid(const FUUID& InGuid) { ActorGuid = InGuid; }

        const std::string& GetClass() const { return ClassName; }
        void SetClass(const std::string& InClassName) { ClassName = InClassName; }

        bool IsPendingKill() const { return bPendingKill; }
        void MarkPendingKill() { bPendingKill = true; }

        void SetName(const std::string& InName) override;

        bool HasBegunPlay() const { return bHasBegunPlay; }
        void MarkBegunPlay() { bHasBegunPlay = true; }
        void ClearBegunPlay() { bHasBegunPlay = false; }

        bool CanEverTick() const { return bCanEverTick; }
        void SetCanEverTick(bool InbCanTick) { bCanEverTick = InbCanTick; }

        ENetRole GetLocalRole() const { return LocalRole; }
        void SetLocalRole(ENetRole InRole) { LocalRole = InRole; }
        bool HasAuthority() const { return LocalRole == ENetRole::Authority; }

        /** True on Standalone/ListenServer authority. False on clients even if a leftover actor still has Authority role. */
        bool IsNetworkAuthority() const {
            if (World && World->GetNetMode() == ENetMode::Client)
                return false;
            return HasAuthority();
        }

        bool GetReplicates() const { return bReplicates; }
        void SetReplicates(bool bInReplicates) { bReplicates = bInReplicates; }
        bool IsAlwaysRelevant() const { return bAlwaysRelevant; }
        void SetAlwaysRelevant(bool bInAlwaysRelevant) { bAlwaysRelevant = bInAlwaysRelevant; }
        float GetNetCullDistanceSquared() const { return NetCullDistanceSquared; }
        void SetNetCullDistanceSquared(float InDistSq) { NetCullDistanceSquared = InDistSq; }

        /** Relevancy v1: AlwaysRelevant, or within NetCullDistanceSquared of the viewer (0 = unlimited). */
        bool IsNetRelevantFor(const glm::vec3& InViewerLocation) const;

        /**
         * @brief Optional extra bytes appended to net snapshots. Override in subclasses.
         */
        virtual void SerializeReplication(std::vector<uint8_t>& OutBytes) const { (void)OutBytes; }
        virtual void DeserializeReplication(const uint8_t* InData, size_t InSize) {
            (void)InData;
            (void)InSize;
        }

        virtual void SerializeControlInput(std::vector<uint8_t>& OutBytes) const { (void)OutBytes; }
        virtual void ApplyControlInput(const uint8_t* InData, size_t InSize) {
            (void)InData;
            (void)InSize;
        }

        /**
         * @brief Queue or execute a ServerRPC (client → authority).
         * Authority calls HandleServerRPC immediately; AutonomousProxy queues on the net connection.
         */
        void CallServerRPC(uint16_t InFunctionId, const std::vector<uint8_t>& InPayload = {});
        /** Authority → owning/all clients. Queues Client kind; does not run locally. */
        void CallClientRPC(uint16_t InFunctionId, const std::vector<uint8_t>& InPayload = {});
        /** Authority → all clients + local HandleClientRPC on listen-server host. */
        void CallMulticastRPC(uint16_t InFunctionId, const std::vector<uint8_t>& InPayload = {});

        virtual bool HandleServerRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) {
            (void)InFunctionId;
            (void)InData;
            (void)InSize;
            return false;
        }
        virtual bool HandleClientRPC(uint16_t InFunctionId, const uint8_t* InData, size_t InSize) {
            (void)InFunctionId;
            (void)InData;
            (void)InSize;
            return false;
        }

        template <typename T> TRef<T> FindActorComponent() const {
            for (const auto& comp : ActorComponents) {
                if (auto typed = std::dynamic_pointer_cast<T>(comp))
                    return typed;
            }
            return nullptr;
        }

        FTransformComponent& GetTransform();
        const FTransformComponent& GetTransform() const;

        /** Unreal RootComponent — typically CapsuleComponent on ACharacter. */
        void SetRootComponent(USceneComponent* NewRoot);
        USceneComponent* GetRootComponent() const { return RootComponent; }

        glm::vec3 GetActorLocation() const;
        void SetActorLocation(const glm::vec3& InLocation);

        glm::vec3 GetActorRotation() const;
        void SetActorRotation(const glm::vec3& InRotation);

        glm::vec3 GetActorScale() const;
        void SetActorScale(const glm::vec3& InScale);

        /** Engine local -Z mapped through the actor transform. */
        glm::vec3 GetActorForwardVector() const;
        glm::vec3 GetActorRightVector() const;
        glm::vec3 GetActorUpVector() const;

        template <typename T, typename... TArgs> TRef<T> AddActorComponent(TArgs&&... InArgs) {
            auto comp = std::make_shared<T>(std::forward<TArgs>(InArgs)...);
            comp->SetOwner(this);
            ActorComponents.push_back(comp);
            if (bHasBegunPlay && !comp->HasBegunPlay()) {
                comp->BeginPlay();
                comp->MarkBegunPlay();
            }
            return comp;
        }

        const std::vector<TRef<UActorComponent>>& GetActorComponents() const { return ActorComponents; }

        template <typename T> T* FindComponentByClass() const {
            for (const auto& comp : ActorComponents) {
                if (T* typed = dynamic_cast<T*>(comp.get()))
                    return typed;
            }
            return nullptr;
        }

        template <typename T, typename... TArgs> T& AddComponent(TArgs&&... InArgs) {
            LE_CORE_ASSERT(World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(!HasComponent<T>(), "Actor already has component!");
            return World->GetRegistry().emplace<T>(EntityHandle, std::forward<TArgs>(InArgs)...);
        }

        template <typename T, typename... TArgs> T& AddOrReplaceComponent(TArgs&&... InArgs) {
            LE_CORE_ASSERT(World != nullptr, "Actor world is null!");
            return World->GetRegistry().emplace_or_replace<T>(EntityHandle, std::forward<TArgs>(InArgs)...);
        }

        template <typename T> T& GetComponent() {
            LE_CORE_ASSERT(World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            return World->GetRegistry().get<T>(EntityHandle);
        }

        template <typename T> const T& GetComponent() const {
            LE_CORE_ASSERT(World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            return World->GetRegistry().get<T>(EntityHandle);
        }

        template <typename T> bool HasComponent() const {
            return World != nullptr && EntityHandle != entt::null && World->GetRegistry().all_of<T>(EntityHandle);
        }

        template <typename T> void RemoveComponent() {
            LE_CORE_ASSERT(World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            World->GetRegistry().remove<T>(EntityHandle);
        }

        operator bool() const { return EntityHandle != entt::null && World != nullptr; }
        operator entt::entity() const { return EntityHandle; }
        operator uint32_t() const { return static_cast<uint32_t>(EntityHandle); }

        bool operator==(const AActor& InOther) const {
            return EntityHandle == InOther.EntityHandle && World == InOther.World;
        }
        bool operator!=(const AActor& InOther) const { return !(*this == InOther); }

    protected:
        entt::entity EntityHandle{entt::null};
        UWorld* World = nullptr;
        FUUID ActorGuid;
        std::string ClassName = "AActor";
        bool bHasBegunPlay = false;
        bool bCanEverTick = true;
        bool bPendingKill = false;
        bool bReplicates = false;
        bool bAlwaysRelevant = false;
        /** Squared cull radius for non-AlwaysRelevant replicating actors. <= 0 means unlimited. */
        float NetCullDistanceSquared = 0.0f;
        ENetRole LocalRole = ENetRole::Authority;
        std::vector<TRef<UActorComponent>> ActorComponents;
        USceneComponent* RootComponent = nullptr;

        friend class UWorld;
        friend class FMapSerializer;
    };

} // namespace Leon
