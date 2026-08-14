#pragma once

#include "core/Base.hpp"
#include "core/Timestep.hpp"
#include "renderer/PerspectiveCamera.hpp"

#include <entt/entt.hpp>
#include <string>

namespace Leon {

    class FEntity;

    class FScene {
    public:
        FScene();
        ~FScene();

        FEntity CreateEntity(const std::string& InName = std::string());
        void DestroyEntity(FEntity InEntity);

        void OnUpdate(FTimestep InTs);
        void OnRender(const FPerspectiveCamera& InCamera);

        void OnViewportResize(uint32_t InWidth, uint32_t InHeight);

        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        static TRef<FScene> Create();

    private:
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;

        TRef<class FFramebuffer> m_ShadowMapFramebuffer;
        TRef<class FFramebuffer> m_PlanarReflectionFramebuffer;
        TRef<class FFramebuffer> m_HDRSceneFramebuffer;
        TRef<class FShader> m_ShadowDepthShader;
        TRef<class FShader> m_SkyboxShader;
        TRef<class FShader> m_PostProcessShader;
        TRef<class FVertexArray> m_SkyboxVA;
        TRef<class FVertexArray> m_FullscreenQuadVA;

        friend class FEntity;
    };

    using Scene = FScene;

} // namespace Leon
