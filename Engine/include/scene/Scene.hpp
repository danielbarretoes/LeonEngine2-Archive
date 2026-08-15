#pragma once

#include "core/Base.hpp"
#include "core/Timestep.hpp"
#include "renderer/PerspectiveCamera.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class FEntity;
    class FSceneRenderer;

    /**
     * @brief Scene data container — owns entities and their components (ECS).
     *
     * FScene is pure data. Rendering is owned by FSceneRenderer.
     * This separation allows multiple render passes, editors, and render graph
     * evolution without bloating the scene class.
     */
    class FScene {
    public:
        FScene();
        ~FScene();

        FEntity CreateEntity(const std::string& InName = std::string());
        void DestroyEntity(FEntity InEntity);

        void OnUpdate(FTimestep InTs);

        /**
         * @brief Execute all render passes for the current frame.
         * Delegates to the internal FSceneRenderer.
         */
        void OnRender(const FPerspectiveCamera& InCamera);

        /**
         * @brief Notify renderer of viewport size change.
         */
        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        FSceneRenderer* GetSceneRenderer() { return m_Renderer.get(); }

        static TRef<FScene> Create();

    private:
        entt::registry m_Registry;
        TScope<FSceneRenderer> m_Renderer; ///< Owns the rendering pipeline

        friend class FEntity;
    };

} // namespace Leon
