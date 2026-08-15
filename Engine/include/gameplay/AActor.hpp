#pragma once

#include "core/Base.hpp"
#include "core/Log.hpp"
#include "gameplay/UObject.hpp"
#include "world/Components.hpp"
#include "world/UWorld.hpp"

#include <entt/entt.hpp>
#include <string>

namespace Leon {

    class UWorld;

    /**
     * @brief Base class for any object placed or spawned within a UWorld.
     *
     * Exactly represents the Unreal Engine AActor:
     * - Participates in the lifecycle: PostInitializeComponents -> BeginPlay -> Tick -> EndPlay -> Destroy
     * - Manages attached components and transform
     * - Belongs to a single UWorld instance
     */
    class AActor : public UObject {
    public:
        AActor() = default;
        AActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Actor");
        ~AActor() override = default;

        // --- Unreal Lifecycle Interface ---
        virtual void PostInitializeComponents() {}
        virtual void BeginPlay() {}
        virtual void Tick(float DeltaSeconds) {}
        virtual void EndPlay() {}
        virtual void Destroy();

        // --- World & Hierarchy Accessors ---
        UWorld* GetWorld() const { return m_World; }
        entt::entity GetEntityHandle() const { return m_EntityHandle; }

        void SetName(const std::string& InName) override;

        bool HasBegunPlay() const { return m_bHasBegunPlay; }
        void MarkBegunPlay() { m_bHasBegunPlay = true; }

        bool CanEverTick() const { return m_bCanEverTick; }
        void SetCanEverTick(bool InbCanTick) { m_bCanEverTick = InbCanTick; }

        // --- Transform & Spatial API ---
        FTransformComponent& GetTransform();
        const FTransformComponent& GetTransform() const;

        glm::vec3 GetActorLocation() const;
        void SetActorLocation(const glm::vec3& InLocation);

        glm::vec3 GetActorRotation() const;
        void SetActorRotation(const glm::vec3& InRotation);

        glm::vec3 GetActorScale() const;
        void SetActorScale(const glm::vec3& InScale);

        // --- Component Template Helpers ---
        template <typename T, typename... TArgs> T& AddComponent(TArgs&&... InArgs) {
            LE_CORE_ASSERT(m_World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(!HasComponent<T>(), "Actor already has component!");
            return m_World->GetRegistry().emplace<T>(m_EntityHandle, std::forward<TArgs>(InArgs)...);
        }

        template <typename T, typename... TArgs> T& AddOrReplaceComponent(TArgs&&... InArgs) {
            LE_CORE_ASSERT(m_World != nullptr, "Actor world is null!");
            return m_World->GetRegistry().emplace_or_replace<T>(m_EntityHandle, std::forward<TArgs>(InArgs)...);
        }

        template <typename T> T& GetComponent() {
            LE_CORE_ASSERT(m_World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            return m_World->GetRegistry().get<T>(m_EntityHandle);
        }

        template <typename T> const T& GetComponent() const {
            LE_CORE_ASSERT(m_World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            return m_World->GetRegistry().get<T>(m_EntityHandle);
        }

        template <typename T> bool HasComponent() const {
            return m_World != nullptr && m_EntityHandle != entt::null && m_World->GetRegistry().all_of<T>(m_EntityHandle);
        }

        template <typename T> void RemoveComponent() {
            LE_CORE_ASSERT(m_World != nullptr, "Actor world is null!");
            LE_CORE_ASSERT(HasComponent<T>(), "Actor does not have component!");
            m_World->GetRegistry().remove<T>(m_EntityHandle);
        }

        operator bool() const { return m_EntityHandle != entt::null && m_World != nullptr; }
        operator entt::entity() const { return m_EntityHandle; }
        operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }

        bool operator==(const AActor& InOther) const {
            return m_EntityHandle == InOther.m_EntityHandle && m_World == InOther.m_World;
        }
        bool operator!=(const AActor& InOther) const { return !(*this == InOther); }

    protected:
        entt::entity m_EntityHandle{entt::null};
        UWorld* m_World = nullptr;
        bool m_bHasBegunPlay = false;
        bool m_bCanEverTick = true;

        friend class UWorld;
        friend class MapSerializer;
    };

} // namespace Leon
