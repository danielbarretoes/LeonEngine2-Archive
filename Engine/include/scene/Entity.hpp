#pragma once

#include "core/Base.hpp"
#include "core/Log.hpp"
#include "scene/Scene.hpp"

#include <entt/entt.hpp>

namespace Leon {

    class FEntity {
    public:
        FEntity() = default;
        FEntity(entt::entity InHandle, FScene* InScene) : m_EntityHandle(InHandle), m_Scene(InScene) {}
        FEntity(const FEntity& InOther) = default;

        template <typename T, typename... TArgs> T& AddComponent(TArgs&&... InArgs) {
            LE_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
            T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<TArgs>(InArgs)...);
            return component;
        }

        template <typename T, typename... TArgs> T& AddOrReplaceComponent(TArgs&&... InArgs) {
            T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<TArgs>(InArgs)...);
            return component;
        }

        template <typename T> T& GetComponent() {
            LE_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template <typename T> const T& GetComponent() const {
            LE_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template <typename T> bool HasComponent() const {
            return m_Scene != nullptr && m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template <typename T> void RemoveComponent() {
            LE_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
            m_Scene->m_Registry.remove<T>(m_EntityHandle);
        }

        operator bool() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }
        operator entt::entity() const { return m_EntityHandle; }
        operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }

        bool operator==(const FEntity& InOther) const {
            return m_EntityHandle == InOther.m_EntityHandle && m_Scene == InOther.m_Scene;
        }

        bool operator!=(const FEntity& InOther) const { return !(*this == InOther); }

    private:
        entt::entity m_EntityHandle{entt::null};
        FScene* m_Scene = nullptr;
    };

    using Entity = FEntity;

} // namespace Leon
