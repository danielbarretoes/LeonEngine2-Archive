#include "scene/Scene.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Shader.hpp"
#include "renderer/TextRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    FScene::FScene() {
        // 1. Initialize 2048x2048 High-Resolution Depth Framebuffer for Directional Shadow Mapping
        FFramebufferSpecification shadowSpec;
        shadowSpec.Width = 2048;
        shadowSpec.Height = 2048;
        shadowSpec.Attachments = {EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_ShadowMapFramebuffer = FFramebuffer::Create(shadowSpec);

        // 2. Initialize Planar Reflection Framebuffer (Offscreen Target)
        FFramebufferSpecification planarSpec;
        planarSpec.Width = 1280;
        planarSpec.Height = 720;
        planarSpec.Attachments = {EFramebufferTextureFormat::RGBA8, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_PlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);

        // 3. Initialize High Dynamic Range (HDR) Scene Framebuffer (16-bit float per channel)
        FFramebufferSpecification hdrSpec;
        hdrSpec.Width = 1280;
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_HDRSceneFramebuffer = FFramebuffer::Create(hdrSpec);

        // 4. Load Shaders, Skybox & Fullscreen Composite Quad
        m_ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");
        m_SkyboxShader = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");
        m_PostProcessShader = FShader::Create("Engine/Assets/Shaders/PostProcess.glsl");

        m_SkyboxVA = FMeshPrimitives::CreateCube(2.0f);
        m_FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);
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

        if (InWidth > 0 && InHeight > 0) {
            if (m_HDRSceneFramebuffer &&
                (m_HDRSceneFramebuffer->GetSpecification().Width != InWidth ||
                 m_HDRSceneFramebuffer->GetSpecification().Height != InHeight)) {
                m_HDRSceneFramebuffer->Resize(InWidth, InHeight);
            }

            if (m_PlanarReflectionFramebuffer &&
                (m_PlanarReflectionFramebuffer->GetSpecification().Width != InWidth ||
                 m_PlanarReflectionFramebuffer->GetSpecification().Height != InHeight)) {
                m_PlanarReflectionFramebuffer->Resize(InWidth, InHeight);
            }

            auto view = m_Registry.view<FCameraComponent>();
            for (auto entity : view) {
                auto& cameraComponent = view.get<FCameraComponent>(entity);
                cameraComponent.Camera.SetViewportSize(InWidth, InHeight);
            }
        }
    }

    void FScene::OnRender(const FPerspectiveCamera& InCamera) {
        // Save the currently bound Framebuffer to restore target on final composite pass
        uint32_t previousFBO = FRenderCommand::GetFramebufferBinding();

        uint32_t vpWidth = m_ViewportWidth > 0 ? m_ViewportWidth : 1280;
        uint32_t vpHeight = m_ViewportHeight > 0 ? m_ViewportHeight : 720;

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

        std::vector<FPointLight> pointLights;
        {
            auto pointLightView = m_Registry.view<FPointLightComponent>();
            for (auto entity : pointLightView) {
                const auto& comp = pointLightView.get<FPointLightComponent>(entity);
                if (comp.bEnabled && pointLights.size() < 16) {
                    FPointLight pl = comp.Light;
                    if (m_Registry.all_of<FTransformComponent>(entity)) {
                        pl.Position = m_Registry.get<FTransformComponent>(entity).Translation;
                    }
                    pointLights.push_back(pl);
                }
            }
        }

        std::vector<FSpotLight> spotLights;
        {
            auto spotLightView = m_Registry.view<FSpotLightComponent>();
            for (auto entity : spotLightView) {
                const auto& comp = spotLightView.get<FSpotLightComponent>(entity);
                if (comp.bEnabled && spotLights.size() < 8) {
                    FSpotLight sl = comp.Light;
                    if (m_Registry.all_of<FTransformComponent>(entity)) {
                        sl.Position = m_Registry.get<FTransformComponent>(entity).Translation;
                    }
                    spotLights.push_back(sl);
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
        // PASS 1: Dynamic Directional Shadow Depth Pre-Pass
        // ==========================================
        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);

        if (bHasDirLight && m_ShadowMapFramebuffer && m_ShadowDepthShader) {
            glm::mat4 lightProjection = glm::ortho(-16.0f, 16.0f, -16.0f, 16.0f, 0.1f, 45.0f);
            glm::vec3 lightDirNorm = glm::normalize(dirLight.Direction);
            glm::vec3 lightPos = -lightDirNorm * 22.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            lightSpaceMatrix = lightProjection * lightView;

            m_ShadowMapFramebuffer->Bind();
            FRenderCommand::SetViewport(0, 0, 2048, 2048);
            FRenderCommand::Clear();

            FRenderCommand::SetDepthTesting(true);
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetCulling(true, ECullMode::Front); // Mitigate shadow acne on closed hulls

            m_ShadowDepthShader->Bind();
            m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

            auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray || !mesh.bCastShadows)
                    continue;

                glm::mat4 model = transform.GetTransform();
                m_ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                mesh.VertexArray->Bind();
                FRenderCommand::DrawIndexed(mesh.VertexArray);
            }

            FRenderCommand::SetCulling(false);
            m_ShadowMapFramebuffer->Unbind();
        }

        // ==========================================
        // PASS 2: Real-time Planar Reflection Pass (Ground Mirror)
        // ==========================================
        if (m_PlanarReflectionFramebuffer) {
            glm::vec3 camPos = InCamera.GetPosition();
            float pitch = InCamera.GetPitch();
            float yaw = InCamera.GetYaw();

            FPerspectiveCamera mirroredCamera = InCamera;
            mirroredCamera.SetPosition(glm::vec3(camPos.x, -camPos.y, camPos.z));
            mirroredCamera.SetRotation(-pitch, yaw);

            m_PlanarReflectionFramebuffer->Bind();
            FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
            FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            FRenderCommand::Clear();
            FRenderCommand::SetDepthTesting(true);
            FRenderCommand::SetDepthMask(true);

            // 2.1 Render Skybox into Planar Reflection Target
            if (skybox.bEnabled && m_SkyboxShader && m_SkyboxVA) {
                FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
                FRenderCommand::SetDepthMask(false);

                m_SkyboxShader->Bind();
                m_SkyboxShader->SetMat4("u_View", glm::value_ptr(mirroredCamera.GetViewMatrix()));
                m_SkyboxShader->SetMat4("u_Projection", glm::value_ptr(mirroredCamera.GetProjectionMatrix()));

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

                m_SkyboxVA->Bind();
                FRenderCommand::DrawIndexed(m_SkyboxVA);

                FRenderCommand::SetDepthMask(true);
                FRenderCommand::SetDepthFunc(EDepthFunc::Less);
            }

            // 2.2 Render 3D Scene Objects sitting above ground plane
            auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisibleInReflection)
                    continue;

                mesh.Shader->Bind();
                mesh.Shader->SetMat4("u_ViewProjection", glm::value_ptr(mirroredCamera.GetViewProjectionMatrix()));
                mesh.Shader->SetFloat3("u_ViewPos", mirroredCamera.GetPosition().x, mirroredCamera.GetPosition().y,
                                       mirroredCamera.GetPosition().z);
                mesh.Shader->SetInt("u_UsePlanarReflection", 0);
                mesh.Shader->SetInt("u_UseShadows", 0);

                if (m_Registry.all_of<FPBRMaterialComponent>(entity)) {
                    const auto& pbrMat = m_Registry.get<FPBRMaterialComponent>(entity).Material;
                    if (pbrMat.bUseAlbedoMap && pbrMat.AlbedoMap && pbrMat.AlbedoMap->IsLoaded()) {
                        pbrMat.AlbedoMap->Bind(0);
                        mesh.Shader->SetInt("u_AlbedoMap", 0);
                        mesh.Shader->SetInt("u_UseAlbedoMap", 1);
                    } else {
                        mesh.Shader->SetInt("u_UseAlbedoMap", 0);
                    }
                    mesh.Shader->SetFloat3("u_AlbedoColor", pbrMat.AlbedoColor.r, pbrMat.AlbedoColor.g,
                                           pbrMat.AlbedoColor.b);
                    mesh.Shader->SetFloat("u_Metallic", pbrMat.Metallic);
                    mesh.Shader->SetFloat("u_Roughness", pbrMat.Roughness);
                    mesh.Shader->SetFloat("u_AO", pbrMat.AO);
                    mesh.Shader->SetInt("u_UseNormalMap", 0);
                    mesh.Shader->SetInt("u_UseMetallicMap", 0);
                    mesh.Shader->SetInt("u_UseRoughnessMap", 0);
                    mesh.Shader->SetInt("u_UseAOMap", 0);
                }

                // Upload Lights to Reflection Buffer
                if (bHasDirLight) {
                    mesh.Shader->SetInt("u_DirLight.enabled", 1);
                    mesh.Shader->SetFloat3("u_DirLight.direction", dirLight.Direction.x, dirLight.Direction.y,
                                           dirLight.Direction.z);
                    mesh.Shader->SetFloat3("u_DirLight.color", dirLight.Color.x, dirLight.Color.y, dirLight.Color.z);
                    mesh.Shader->SetFloat("u_DirLight.ambientIntensity", dirLight.AmbientIntensity);
                    mesh.Shader->SetFloat("u_DirLight.diffuseIntensity", dirLight.DiffuseIntensity);
                    mesh.Shader->SetFloat("u_DirLight.specularIntensity", dirLight.SpecularIntensity);
                }

                mesh.Shader->SetInt("u_PointLightCount", static_cast<int>(pointLights.size()));
                for (size_t i = 0; i < pointLights.size(); ++i) {
                    std::string base = "u_PointLights[" + std::to_string(i) + "].";
                    mesh.Shader->SetInt(base + "enabled", 1);
                    mesh.Shader->SetFloat3(base + "position", pointLights[i].Position.x, pointLights[i].Position.y,
                                           pointLights[i].Position.z);
                    mesh.Shader->SetFloat3(base + "color", pointLights[i].Color.x, pointLights[i].Color.y,
                                           pointLights[i].Color.z);
                    mesh.Shader->SetFloat(base + "constant", pointLights[i].Constant);
                    mesh.Shader->SetFloat(base + "linear", pointLights[i].Linear);
                    mesh.Shader->SetFloat(base + "quadratic", pointLights[i].Quadratic);
                    mesh.Shader->SetFloat(base + "ambientIntensity", pointLights[i].AmbientIntensity);
                    mesh.Shader->SetFloat(base + "diffuseIntensity", pointLights[i].DiffuseIntensity);
                    mesh.Shader->SetFloat(base + "specularIntensity", pointLights[i].SpecularIntensity);
                }

                mesh.Shader->SetFloat3("u_EnvSkyColor", skybox.SkyZenithColor.r, skybox.SkyZenithColor.g,
                                       skybox.SkyZenithColor.b);
                mesh.Shader->SetFloat3("u_EnvHorizonColor", skybox.HorizonColor.r, skybox.HorizonColor.g,
                                       skybox.HorizonColor.b);
                mesh.Shader->SetFloat3("u_EnvGroundColor", skybox.GroundColor.r, skybox.GroundColor.g,
                                       skybox.GroundColor.b);
                mesh.Shader->SetFloat("u_EnvIntensity", skybox.EnvironmentIntensity);

                glm::mat4 model = transform.GetTransform();
                mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
                mesh.VertexArray->Bind();
                FRenderCommand::DrawIndexed(mesh.VertexArray);
            }

            // 2.3 Render 3D Text Actors into Planar Reflection
            auto textView = m_Registry.view<FTransformComponent, FTextComponent>();
            FTextRenderer::BeginScene(mirroredCamera);
            for (auto entity : textView) {
                auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
                if (textComp.Text.empty())
                    continue;

                glm::mat4 model = transform.GetTransform();
                FTextRenderer::DrawString(textComp.Text, model, textComp.Color, textComp.Size, textComp.Alignment,
                                          textComp.bDoubleSided);
            }
            FTextRenderer::EndScene();

            m_PlanarReflectionFramebuffer->Unbind();
        }

        // ==========================================
        // PASS 3: Main Scene HDR Render Pass (Linear High Dynamic Range)
        // ==========================================
        if (m_HDRSceneFramebuffer) {
            m_HDRSceneFramebuffer->Bind();
        } else {
            FRenderCommand::BindFramebuffer(previousFBO);
        }

        FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);

        FRenderer::BeginScene(InCamera);

        auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);

            if (!mesh.VertexArray || !mesh.Shader) {
                continue;
            }

            mesh.Shader->Bind();

            // Camera ViewProjection & Position
            mesh.Shader->SetMat4("u_ViewProjection", glm::value_ptr(InCamera.GetViewProjectionMatrix()));
            mesh.Shader->SetFloat3("u_ViewPos", InCamera.GetPosition().x, InCamera.GetPosition().y,
                                   InCamera.GetPosition().z);

            // Dynamic Directional Shadow Map (Slot 5)
            mesh.Shader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));
            if (m_ShadowMapFramebuffer && bHasDirLight && mesh.bReceiveShadows) {
                m_ShadowMapFramebuffer->BindDepthTexture(5);
                mesh.Shader->SetInt("u_ShadowMap", 5);
                mesh.Shader->SetInt("u_UseShadows", 1);
            } else {
                mesh.Shader->SetInt("u_UseShadows", 0);
            }

            // Planar Ground Reflection Map (Slot 6)
            bool bApplyPlanarReflection = false;
            if (m_Registry.all_of<FPBRMaterialComponent>(entity)) {
                const auto& pbrMat = m_Registry.get<FPBRMaterialComponent>(entity).Material;
                if (pbrMat.bUsePlanarReflection && m_PlanarReflectionFramebuffer) {
                    bApplyPlanarReflection = true;
                }
            }

            if (bApplyPlanarReflection) {
                m_PlanarReflectionFramebuffer->BindTexture(0, 6);
                mesh.Shader->SetInt("u_PlanarReflectionMap", 6);
                mesh.Shader->SetInt("u_UsePlanarReflection", 1);
                mesh.Shader->SetFloat2("u_ScreenSize", static_cast<float>(vpWidth), static_cast<float>(vpHeight));
            } else {
                mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            }

            // Environment / IBL Atmosphere Parameters
            mesh.Shader->SetFloat3("u_EnvSkyColor", skybox.SkyZenithColor.r, skybox.SkyZenithColor.g,
                                   skybox.SkyZenithColor.b);
            mesh.Shader->SetFloat3("u_EnvHorizonColor", skybox.HorizonColor.r, skybox.HorizonColor.g,
                                   skybox.HorizonColor.b);
            mesh.Shader->SetFloat3("u_EnvGroundColor", skybox.GroundColor.r, skybox.GroundColor.g,
                                   skybox.GroundColor.b);
            mesh.Shader->SetFloat("u_EnvIntensity", skybox.EnvironmentIntensity);

            // Directional Light Uniforms
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

            // Multi Point Light Uniforms
            mesh.Shader->SetInt("u_PointLightCount", static_cast<int>(pointLights.size()));
            for (size_t i = 0; i < pointLights.size(); ++i) {
                std::string base = "u_PointLights[" + std::to_string(i) + "].";
                mesh.Shader->SetInt(base + "enabled", 1);
                mesh.Shader->SetFloat3(base + "position", pointLights[i].Position.x, pointLights[i].Position.y,
                                       pointLights[i].Position.z);
                mesh.Shader->SetFloat3(base + "color", pointLights[i].Color.x, pointLights[i].Color.y,
                                       pointLights[i].Color.z);
                mesh.Shader->SetFloat(base + "constant", pointLights[i].Constant);
                mesh.Shader->SetFloat(base + "linear", pointLights[i].Linear);
                mesh.Shader->SetFloat(base + "quadratic", pointLights[i].Quadratic);
                mesh.Shader->SetFloat(base + "ambientIntensity", pointLights[i].AmbientIntensity);
                mesh.Shader->SetFloat(base + "diffuseIntensity", pointLights[i].DiffuseIntensity);
                mesh.Shader->SetFloat(base + "specularIntensity", pointLights[i].SpecularIntensity);
            }

            // Multi Spot Light Uniforms
            mesh.Shader->SetInt("u_SpotLightCount", static_cast<int>(spotLights.size()));
            for (size_t i = 0; i < spotLights.size(); ++i) {
                std::string base = "u_SpotLights[" + std::to_string(i) + "].";
                mesh.Shader->SetInt(base + "enabled", 1);
                mesh.Shader->SetFloat3(base + "position", spotLights[i].Position.x, spotLights[i].Position.y,
                                       spotLights[i].Position.z);
                mesh.Shader->SetFloat3(base + "direction", spotLights[i].Direction.x, spotLights[i].Direction.y,
                                       spotLights[i].Direction.z);
                mesh.Shader->SetFloat3(base + "color", spotLights[i].Color.x, spotLights[i].Color.y,
                                       spotLights[i].Color.z);
                mesh.Shader->SetFloat(base + "cutOff", std::cos(glm::radians(spotLights[i].CutOff)));
                mesh.Shader->SetFloat(base + "outerCutOff", std::cos(glm::radians(spotLights[i].OuterCutOff)));
                mesh.Shader->SetFloat(base + "constant", spotLights[i].Constant);
                mesh.Shader->SetFloat(base + "linear", spotLights[i].Linear);
                mesh.Shader->SetFloat(base + "quadratic", spotLights[i].Quadratic);
                mesh.Shader->SetFloat(base + "ambientIntensity", spotLights[i].AmbientIntensity);
                mesh.Shader->SetFloat(base + "diffuseIntensity", spotLights[i].DiffuseIntensity);
                mesh.Shader->SetFloat(base + "specularIntensity", spotLights[i].SpecularIntensity);
            }

            // PBR Material Component Configuration
            if (m_Registry.all_of<FPBRMaterialComponent>(entity)) {
                const auto& pbrMat = m_Registry.get<FPBRMaterialComponent>(entity).Material;

                // Slot 0: Albedo Map
                if (pbrMat.bUseAlbedoMap && pbrMat.AlbedoMap && pbrMat.AlbedoMap->IsLoaded()) {
                    pbrMat.AlbedoMap->Bind(0);
                    mesh.Shader->SetInt("u_AlbedoMap", 0);
                    mesh.Shader->SetInt("u_UseAlbedoMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseAlbedoMap", 0);
                }
                mesh.Shader->SetFloat3("u_AlbedoColor", pbrMat.AlbedoColor.r, pbrMat.AlbedoColor.g,
                                       pbrMat.AlbedoColor.b);

                // Slot 1: Normal Map
                if (pbrMat.bUseNormalMap && pbrMat.NormalMap && pbrMat.NormalMap->IsLoaded()) {
                    pbrMat.NormalMap->Bind(1);
                    mesh.Shader->SetInt("u_NormalMap", 1);
                    mesh.Shader->SetInt("u_UseNormalMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseNormalMap", 0);
                }

                // Slot 2: Metallic Map
                if (pbrMat.bUseMetallicMap && pbrMat.MetallicMap && pbrMat.MetallicMap->IsLoaded()) {
                    pbrMat.MetallicMap->Bind(2);
                    mesh.Shader->SetInt("u_MetallicMap", 2);
                    mesh.Shader->SetInt("u_UseMetallicMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseMetallicMap", 0);
                }
                mesh.Shader->SetFloat("u_Metallic", pbrMat.Metallic);

                // Slot 3: AO Map
                if (pbrMat.bUseAOMap && pbrMat.AOMap && pbrMat.AOMap->IsLoaded()) {
                    pbrMat.AOMap->Bind(3);
                    mesh.Shader->SetInt("u_AOMap", 3);
                    mesh.Shader->SetInt("u_UseAOMap", 1);
                } else {
                    mesh.Shader->SetInt("u_UseAOMap", 0);
                }
                mesh.Shader->SetFloat("u_AO", pbrMat.AO);

                // Slot 4: Roughness Map
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

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // ==========================================
        // 4. Atmospheric Skybox Background Pass
        // ==========================================
        if (skybox.bEnabled && m_SkyboxShader && m_SkyboxVA) {
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetDepthMask(false);

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

            m_SkyboxVA->Bind();
            FRenderCommand::DrawIndexed(m_SkyboxVA);

            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        }

        // ==========================================
        // 5. 3D In-World Text Actors Pass
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

        if (m_HDRSceneFramebuffer) {
            m_HDRSceneFramebuffer->Unbind();

            // ==========================================
            // PASS 4: Post-Processing & Composite Pass (ACES Tonemapping & Gamma)
            // ==========================================
            FRenderCommand::BindFramebuffer(previousFBO);
            FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
            FRenderCommand::SetDepthTesting(false);
            FRenderCommand::SetDepthMask(false);

            if (m_PostProcessShader && m_FullscreenQuadVA) {
                m_PostProcessShader->Bind();
                m_HDRSceneFramebuffer->BindTexture(0, 0);
                m_PostProcessShader->SetInt("u_SceneTexture", 0);
                m_PostProcessShader->SetFloat("u_Exposure", skybox.Exposure);

                m_FullscreenQuadVA->Bind();
                FRenderCommand::DrawIndexed(m_FullscreenQuadVA);
            }

            FRenderCommand::SetDepthTesting(true);
            FRenderCommand::SetDepthMask(true);
        }
    }

} // namespace Leon
