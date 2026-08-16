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
#include <memory>
#include <string>
#include <vector>

namespace Leon {

    class UWorld;

    /**
     * @brief Base class for any object placed or spawned within a UWorld.
     *
     * Lifecycle: PostInitializeComponents -> BeginPlay -> Tick -> EndPlay -> Destroy
     * Owns optional lite UActorComponent list (logic). EnTT POD comps via AddComponent<T>().
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

        FTransformComponent& GetTransform();
        const FTransformComponent& GetTransform() const;

        glm::vec3 GetActorLocation() const;
        void SetActorLocation(const glm::vec3& InLocation);

        glm::vec3 GetActorRotation() const;
        void SetActorRotation(const glm::vec3& InRotation);

        glm::vec3 GetActorScale() const;
        void SetActorScale(const glm::vec3& InScale);

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
            return World != nullptr && EntityHandle != entt::null &&
                   World->GetRegistry().all_of<T>(EntityHandle);
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
        ENetRole LocalRole = ENetRole::Authority;
        std::vector<TRef<UActorComponent>> ActorComponents;

        friend class UWorld;
        friend class FMapSerializer;
    };

} // namespace Leon
