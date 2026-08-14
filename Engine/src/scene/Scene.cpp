#include "scene/Scene.hpp"
#include "core/Log.hpp"
#include "renderer/SceneRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"

namespace Leon {

    FScene::FScene() {
        // FScene is pure ECS data — rendering is fully owned by FSceneRenderer.
        m_Renderer = MakeScope<FSceneRenderer>(this);
    }

    FScene::~FScene() {
        m_Registry.clear();
    }

    TRef<FScene> FScene::Create() {
        return MakeRef<FScene>();
    }

    FEntity FScene::CreateEntity(const std::string& InName) {
        FEntity entity = {m_Registry.create(), this};
        entity.AddComponent<FTransformComponent>();
        auto& tag = entity.AddComponent<FTagComponent>();
        tag.Tag = InName.empty() ? "Entity" : InName;
        return entity;
    }

    void FScene::DestroyEntity(FEntity InEntity) {
        m_Registry.destroy(InEntity);
    }

    void FScene::OnUpdate(FTimestep InTs) {
        // Reserved for scripts, physics, or entity lifecycle updates.
    }

    void FScene::OnRender(const FPerspectiveCamera& InCamera) {
        if (m_Renderer)
            m_Renderer->Render(InCamera);
    }

    void FScene::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        if (InWidth > 0 && InHeight > 0) {
            // Update any camera components
            auto view = m_Registry.view<FCameraComponent>();
            for (auto entity : view) {
                view.get<FCameraComponent>(entity).Camera.SetViewportSize(InWidth, InHeight);
            }

            // Resize renderer framebuffers
            if (m_Renderer)
                m_Renderer->OnViewportResize(InWidth, InHeight);
        }
    }

} // namespace Leon
