#include "scene/Scene.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Shader.hpp"
#include "renderer/TextRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"

#include <cmath>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    FScene::FScene() {
        // Initialize 2048x2048 high-resolution Depth Framebuffer for Directional Shadow Mapping
        FFramebufferSpecification shadowSpec;
        shadowSpec.Width = 2048;
        shadowSpec.Height = 2048;
        shadowSpec.Attachments = {EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_ShadowMapFramebuffer = FFramebuffer::Create(shadowSpec);

        // Load Shadow Depth Pre-pass Shader
        m_ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");

        // Load Atmospheric HDR Skybox Shader & Cube Mesh
        m_SkyboxShader = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");
        m_SkyboxVA = FMeshPrimitives::CreateCube(2.0f);
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
        // Reserved for scripts, physics, or entity component lifecycle updates
    }

    void FScene::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        m_ViewportWidth = InWidth;
        m_ViewportHeight = InHeight;

        // Resize cameras in scene
        auto view = m_Registry.view<FCameraComponent>();
        for (auto entity : view) {
            auto& cameraComponent = view.get<FCameraComponent>(entity);
            if (InWidth > 0 && InHeight > 0) {
                cameraComponent.Camera.SetViewportSize(InWidth, InHeight);
            }
        }
    }

    void FScene::OnRender(const FPerspectiveCamera& InCamera) {
        // Save the currently bound Framebuffer so shadow pass doesn't override offscreen targets
        GLint previousFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

        // 1. Gather Light Sources and Environment Settings from the Scene Registry
        bool bHasDirLight = false;
        FDirectionalLight dirLight;
        {
            auto dirLightView = m_Registry.view<FDirectionalLightComponent>();
            for (auto entity : dirLightView) {
                const auto& comp = dirLightView.get<FDirectionalLightComponent>(entity);
                if (comp.bEnabled) {
                    dirLight = comp.Light;
                    bHasDirLight = true;
                    break;
                }
            }
        }

        bool bHasPointLight = false;
        FPointLight pointLight;
        {
            auto pointLightView = m_Registry.view<FPointLightComponent>();
            for (auto entity : pointLightView) {
                const auto& comp = pointLightView.get<FPointLightComponent>(entity);
                if (comp.bEnabled) {
                    pointLight = comp.Light;
                    if (m_Registry.all_of<FTransformComponent>(entity)) {
                        pointLight.Position = m_Registry.get<FTransformComponent>(entity).Translation;
                    }
                    bHasPointLight = true;
                    break;
                }
            }
        }

        bool bHasSpotLight = false;
        FSpotLight spotLight;
        {
            auto spotLightView = m_Registry.view<FSpotLightComponent>();
            for (auto entity : spotLightView) {
                const auto& comp = spotLightView.get<FSpotLightComponent>(entity);
                if (comp.bEnabled) {
                    spotLight = comp.Light;
                    if (m_Registry.all_of<FTransformComponent>(entity)) {
                        spotLight.Position = m_Registry.get<FTransformComponent>(entity).Translation;
                    }
                    bHasSpotLight = true;
                    break;
                }
            }
        }

        FSkyboxComponent skybox;
        {
            auto skyboxView = m_Registry.view<FSkyboxComponent>();
            for (auto entity : skyboxView) {
                skybox = skyboxView.get<FSkyboxComponent>(entity);
                break;
            }
        }

        // ==========================================
        // 2. Dynamic Shadow Depth Pre-Pass
        // ==========================================
        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);

        if (bHasDirLight && m_ShadowMapFramebuffer && m_ShadowDepthShader) {
            // Orthographic projection matrix covering the scene bounds
            glm::mat4 lightProjection = glm::ortho(-12.0f, 12.0f, -12.0f, 12.0f, 0.1f, 35.0f);

            glm::vec3 lightDirNorm = glm::normalize(dirLight.Direction);
            glm::vec3 lightPos = -lightDirNorm * 15.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            lightSpaceMatrix = lightProjection * lightView;

            // Bind Shadow Framebuffer
            m_ShadowMapFramebuffer->Bind();
            glViewport(0, 0, 2048, 2048);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Front-face culling during shadow pass to prevent Peter Panning / self-shadow artifacts
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            m_ShadowDepthShader->Bind();
            m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

            auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray) {
                    continue;
                }

                glm::mat4 model = transform.GetTransform();
                m_ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                mesh.VertexArray->Bind();
                FRenderCommand::DrawIndexed(mesh.VertexArray);
            }

            // Restore normal back-face culling
            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);

            m_ShadowMapFramebuffer->Unbind();
        }

        // ==========================================
        // 3. Main Scene Rendering Pass (PBR & Blinn-Phong)
        // ==========================================
        // Restore target Framebuffer and Viewport
        glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);

        uint32_t vpWidth = m_ViewportWidth > 0 ? m_ViewportWidth : 1280;
        uint32_t vpHeight = m_ViewportHeight > 0 ? m_ViewportHeight : 720;
        glViewport(0, 0, vpWidth, vpHeight);

        FRenderer::BeginScene(InCamera);

        auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);

            if (!mesh.VertexArray || !mesh.Shader) {
                continue;
            }

            mesh.Shader->Bind();

            // Upload Camera ViewPos
            mesh.Shader->SetFloat3("u_ViewPos", InCamera.GetPosition().x, InCamera.GetPosition().y,
                                   InCamera.GetPosition().z);

            // Upload Light Space Matrix and Bind Shadow Depth Texture to Slot 3
            mesh.Shader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));
            if (m_ShadowMapFramebuffer && bHasDirLight) {
                m_ShadowMapFramebuffer->BindDepthTexture(3);
                mesh.Shader->SetInt("u_ShadowMap", 3);
                mesh.Shader->SetInt("u_UseShadows", 1);
            } else {
                mesh.Shader->SetInt("u_UseShadows", 0);
            }

            // Upload Environment / IBL Atmosphere Parameters
            mesh.Shader->SetFloat3("u_EnvSkyColor", skybox.SkyZenithColor.r, skybox.SkyZenithColor.g,
                                   skybox.SkyZenithColor.b);
            mesh.Shader->SetFloat3("u_EnvHorizonColor", skybox.HorizonColor.r, skybox.HorizonColor.g,
                                   skybox.HorizonColor.b);
            mesh.Shader->SetFloat3("u_EnvGroundColor", skybox.GroundColor.r, skybox.GroundColor.g,
                                   skybox.GroundColor.b);
            mesh.Shader->SetFloat("u_EnvIntensity", skybox.EnvironmentIntensity);
            mesh.Shader->SetFloat("u_Exposure", skybox.Exposure);

            // Upload Directional Light Uniforms
            if (bHasDirLight) {
                mesh.Shader->SetInt("u_DirLight.enabled", 1);
                mesh.Shader->SetFloat3("u_DirLight.direction", dirLight.Direction.x, dirLight.Direction.y,
                                       dirLight.Direction.z);
                mesh.Shader->SetFloat3("u_DirLight.color", dirLight.Color.x, dirLight.Color.y, dirLight.Color.z);
                mesh.Shader->SetFloat("u_DirLight.ambientIntensity", dirLight.AmbientIntensity);
                mesh.Shader->SetFloat("u_DirLight.diffuseIntensity", dirLight.DiffuseIntensity);
                mesh.Shader->SetFloat("u_DirLight.specularIntensity", dirLight.SpecularIntensity);
            } else {
                mesh.Shader->SetInt("u_DirLight.enabled", 0);
            }

            // Upload Point Light Uniforms
            if (bHasPointLight) {
                mesh.Shader->SetInt("u_PointLight.enabled", 1);
                mesh.Shader->SetFloat3("u_PointLight.position", pointLight.Position.x, pointLight.Position.y,
                                       pointLight.Position.z);
                mesh.Shader->SetFloat3("u_PointLight.color", pointLight.Color.x, pointLight.Color.y,
                                       pointLight.Color.z);
                mesh.Shader->SetFloat("u_PointLight.constant", pointLight.Constant);
                mesh.Shader->SetFloat("u_PointLight.linear", pointLight.Linear);
                mesh.Shader->SetFloat("u_PointLight.quadratic", pointLight.Quadratic);
                mesh.Shader->SetFloat("u_PointLight.ambientIntensity", pointLight.AmbientIntensity);
                mesh.Shader->SetFloat("u_PointLight.diffuseIntensity", pointLight.DiffuseIntensity);
                mesh.Shader->SetFloat("u_PointLight.specularIntensity", pointLight.SpecularIntensity);
            } else {
                mesh.Shader->SetInt("u_PointLight.enabled", 0);
            }

            // Upload Spot Light Uniforms
            if (bHasSpotLight) {
                mesh.Shader->SetInt("u_SpotLight.enabled", 1);
                mesh.Shader->SetFloat3("u_SpotLight.position", spotLight.Position.x, spotLight.Position.y,
                                       spotLight.Position.z);
                mesh.Shader->SetFloat3("u_SpotLight.direction", spotLight.Direction.x, spotLight.Direction.y,
                                       spotLight.Direction.z);
                mesh.Shader->SetFloat3("u_SpotLight.color", spotLight.Color.x, spotLight.Color.y, spotLight.Color.z);
                mesh.Shader->SetFloat("u_SpotLight.cutOff", std::cos(glm::radians(spotLight.CutOff)));
                mesh.Shader->SetFloat("u_SpotLight.outerCutOff", std::cos(glm::radians(spotLight.OuterCutOff)));
                mesh.Shader->SetFloat("u_SpotLight.constant", spotLight.Constant);
                mesh.Shader->SetFloat("u_SpotLight.linear", spotLight.Linear);
                mesh.Shader->SetFloat("u_SpotLight.quadratic", spotLight.Quadratic);
                mesh.Shader->SetFloat("u_SpotLight.ambientIntensity", spotLight.AmbientIntensity);
                mesh.Shader->SetFloat("u_SpotLight.diffuseIntensity", spotLight.DiffuseIntensity);
                mesh.Shader->SetFloat("u_SpotLight.specularIntensity", spotLight.SpecularIntensity);
            } else {
                mesh.Shader->SetInt("u_SpotLight.enabled", 0);
            }

            // Check for PBR Material Component
            if (m_Registry.all_of<FPBRMaterialComponent>(entity)) {
                const auto& pbrMat = m_Registry.get<FPBRMaterialComponent>(entity).Material;

                // Albedo Map / Color
                if (pbrMat.bUseAlbedoMap && pbrMat.AlbedoMap && pbrMat.AlbedoMap->IsLoaded()) {
                    pbrMat.AlbedoMap->Bind(0);
                    mesh.Shader->SetInt("u_AlbedoMap", 0);
                    mesh.Shader->SetInt("u_UseAlbedoMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseAlbedoMap", 0);
                }
                mesh.Shader->SetFloat3("u_AlbedoColor", pbrMat.AlbedoColor.r, pbrMat.AlbedoColor.g,
                                       pbrMat.AlbedoColor.b);

                // Normal Map
                if (pbrMat.bUseNormalMap && pbrMat.NormalMap && pbrMat.NormalMap->IsLoaded()) {
                    pbrMat.NormalMap->Bind(1);
                    mesh.Shader->SetInt("u_NormalMap", 1);
                    mesh.Shader->SetInt("u_UseNormalMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseNormalMap", 0);
                }

                // Metallic Map / Scalar
                if (pbrMat.bUseMetallicMap && pbrMat.MetallicMap && pbrMat.MetallicMap->IsLoaded()) {
                    pbrMat.MetallicMap->Bind(2);
                    mesh.Shader->SetInt("u_MetallicMap", 2);
                    mesh.Shader->SetInt("u_UseMetallicMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseMetallicMap", 0);
                }
                mesh.Shader->SetFloat("u_Metallic", pbrMat.Metallic);

                // Roughness Map / Scalar
                if (pbrMat.bUseRoughnessMap && pbrMat.RoughnessMap && pbrMat.RoughnessMap->IsLoaded()) {
                    pbrMat.RoughnessMap->Bind(4);
                    mesh.Shader->SetInt("u_RoughnessMap", 4);
                    mesh.Shader->SetInt("u_UseRoughnessMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseRoughnessMap", 0);
                }
                mesh.Shader->SetFloat("u_Roughness", pbrMat.Roughness);
            } else {
                // Classic Blinn-Phong Diffuse Texture
                if (mesh.bUseTexture && mesh.Texture && mesh.Texture->IsLoaded()) {
                    mesh.Texture->Bind(0);
                    mesh.Shader->SetInt("u_DiffuseMap", 0);
                    mesh.Shader->SetInt("u_UseTexture", 1);
                } else {
                    mesh.Shader->SetInt("u_UseTexture", 0);
                }
            }

            // Upload Model Transformation Matrix
            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));

            // Submit Mesh for indexed rendering
            FRenderer::SubmitIndexed(mesh.Shader, mesh.VertexArray);
        }

        FRenderer::EndScene();

        // ==========================================
        // 4. Atmospheric Skybox Background Pass
        // ==========================================
        if (skybox.bEnabled && m_SkyboxShader && m_SkyboxVA) {
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);

            m_SkyboxShader->Bind();
            m_SkyboxShader->SetMat4("u_View", glm::value_ptr(InCamera.GetViewMatrix()));
            m_SkyboxShader->SetMat4("u_Projection", glm::value_ptr(InCamera.GetProjectionMatrix()));

            glm::vec3 sunDir = bHasDirLight ? -glm::normalize(dirLight.Direction) : glm::vec3(0.0f, 1.0f, 0.0f);

            m_SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
            m_SkyboxShader->SetFloat3("u_SkyColor", skybox.SkyZenithColor.r, skybox.SkyZenithColor.g,
                                      skybox.SkyZenithColor.b);
            m_SkyboxShader->SetFloat3("u_HorizonColor", skybox.HorizonColor.r, skybox.HorizonColor.g,
                                      skybox.HorizonColor.b);
            m_SkyboxShader->SetFloat3("u_GroundColor", skybox.GroundColor.r, skybox.GroundColor.g,
                                      skybox.GroundColor.b);
            m_SkyboxShader->SetFloat3("u_SunColor", skybox.SunColor.r, skybox.SunColor.g, skybox.SunColor.b);
            m_SkyboxShader->SetFloat("u_SunIntensity", skybox.SunIntensity);
            m_SkyboxShader->SetFloat("u_Exposure", skybox.Exposure);

            m_SkyboxVA->Bind();
            FRenderCommand::DrawIndexed(m_SkyboxVA);

            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }

        // ==========================================
        // 5. 3D In-World Text Actors Pass (Unreal Engine ATextRenderActor style)
        // ==========================================
        auto textView = m_Registry.view<FTransformComponent, FTextComponent>();
        FTextRenderer::BeginScene(InCamera);

        for (auto entity : textView) {
            auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
            if (textComp.Text.empty())
                continue;

            glm::mat4 model = transform.GetTransform();
            FTextRenderer::DrawString(textComp.Text, model, textComp.Color, textComp.Size, textComp.Alignment,
                                      textComp.bDoubleSided);
        }

        FTextRenderer::EndScene();
    }

} // namespace Leon
