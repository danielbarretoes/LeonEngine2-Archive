#include "scene/Scene.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Shader.hpp"
#include "renderer/TextRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace Leon {

    FScene::FScene() {
        // 1. Initialize 3 Cascaded Shadow Map Depth Framebuffers (2048x2048 each)
        for (int i = 0; i < 3; ++i) {
            FFramebufferSpecification csmSpec;
            csmSpec.Width = 2048;
            csmSpec.Height = 2048;
            csmSpec.Attachments = {EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW};
            m_CascadeShadowFramebuffers[i] = FFramebuffer::Create(csmSpec);
        }

        // 2. Initialize Spot Light Shadow Depth Framebuffer (1024x1024)
        FFramebufferSpecification spotSpec;
        spotSpec.Width = 1024;
        spotSpec.Height = 1024;
        spotSpec.Attachments = {EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW};
        m_SpotShadowFramebuffer = FFramebuffer::Create(spotSpec);

        // 3. Initialize Planar Reflection Framebuffer (Offscreen Target)
        FFramebufferSpecification planarSpec;
        planarSpec.Width = 1280;
        planarSpec.Height = 720;
        planarSpec.Attachments = {EFramebufferTextureFormat::RGBA8, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_PlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);

        // 4. Initialize High Dynamic Range (HDR) Scene Framebuffer (16-bit float per channel)
        FFramebufferSpecification hdrSpec;
        hdrSpec.Width = 1280;
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_HDRSceneFramebuffer = FFramebuffer::Create(hdrSpec);

        // 5. Initialize std140 Uniform Buffer Objects (UBOs)
        // Binding 0: Camera Data (416 bytes)
        m_CameraUBO = FUniformBuffer::Create(sizeof(FCameraBufferData), 0);
        // Binding 1: Lighting Data (1776 bytes)
        m_LightingUBO = FUniformBuffer::Create(sizeof(FLightingBufferData), 1);

        // 6. Load Shaders, Skybox & Fullscreen Composite Quad
        m_ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");
        m_SkyboxShader = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");
        m_PostProcessShader = FShader::Create("Engine/Assets/Shaders/PostProcess.glsl");

        m_SkyboxVA = FMeshPrimitives::CreateCube(2.0f);
        m_FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);

        // 7. Initialize Initial IBL Environment & 2D BRDF LUT
        m_IBLEnvironment = FIBLGenerator::CreateEnvironmentFromSkybox(FSkyboxComponent{});
        m_bEnvironmentGenerated = true;
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

    void FScene::UpdateIBL(const FSkyboxComponent& InSkybox) {
        std::string currentHdr = InSkybox.HDREnvironmentMapPath;
        if (currentHdr.empty() && InSkybox.HDREnvironmentMap) {
            currentHdr = InSkybox.HDREnvironmentMap->GetPath();
        }

        if (InSkybox.bUseHDREnvironmentMap && !currentHdr.empty() && currentHdr != m_LoadedHDRPath) {
            m_IBLEnvironment = FIBLGenerator::CreateEnvironmentFromSkybox(InSkybox);
            m_LoadedHDRPath = currentHdr;
            m_bEnvironmentGenerated = true;
        } else if (!InSkybox.bUseHDREnvironmentMap && !m_LoadedHDRPath.empty()) {
            m_IBLEnvironment = FIBLGenerator::CreateEnvironmentFromSkybox(InSkybox);
            m_LoadedHDRPath = "";
            m_bEnvironmentGenerated = true;
        }
    }

    void FScene::RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                          const FDirectionalLightComponent* InDirLightComp,
                                          FCameraBufferData& OutCamData) {
        if (!InDirLightComp || !InDirLightComp->bEnabled || !m_ShadowDepthShader)
            return;

        float nearClip = InCamera.GetNearClip();
        float farClip = 75.0f;

        float split0 = 4.5f;
        float split1 = 20.0f;
        float split2 = farClip;

        OutCamData.CascadeSplits = glm::vec4(split0, split1, split2, 0.0f);

        float cascadeSplitsNear[3] = {nearClip, split0, split1};
        float cascadeSplitsFar[3] = {split0, split1, split2};

        glm::vec3 lightDirNorm = glm::normalize(InDirLightComp->Light.Direction);

        for (int cascade = 0; cascade < 3; ++cascade) {
            if (!m_CascadeShadowFramebuffers[cascade])
                continue;

            float fov = InCamera.GetFOV();
            float aspect = InCamera.GetAspectRatio();
            glm::mat4 subProj = glm::perspective(glm::radians(fov), aspect, cascadeSplitsNear[cascade],
                                                 cascadeSplitsFar[cascade]);
            glm::mat4 invSubVP = glm::inverse(subProj * InCamera.GetViewMatrix());

            std::vector<glm::vec4> frustumCorners;
            frustumCorners.reserve(8);
            for (unsigned int x = 0; x < 2; ++x) {
                for (unsigned int y = 0; y < 2; ++y) {
                    for (unsigned int z = 0; z < 2; ++z) {
                        glm::vec4 pt =
                            invSubVP * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                        frustumCorners.push_back(pt / pt.w);
                    }
                }
            }

            glm::vec3 center(0.0f);
            for (const auto& v : frustumCorners)
                center += glm::vec3(v);
            center /= 8.0f;

            glm::vec3 lightPos = center - lightDirNorm * 40.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();

            for (const auto& v : frustumCorners) {
                glm::vec4 trf = lightView * v;
                minX = std::min(minX, trf.x);
                maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y);
                maxY = std::max(maxY, trf.y);
                minZ = std::min(minZ, trf.z);
                maxZ = std::max(maxZ, trf.z);
            }

            float zMargin = 30.0f;
            minZ -= zMargin;
            maxZ += zMargin;

            float texelSizeX = (maxX - minX) / 2048.0f;
            float texelSizeY = (maxY - minY) / 2048.0f;
            minX = std::floor(minX / texelSizeX) * texelSizeX;
            maxX = std::floor(maxX / texelSizeX) * texelSizeX;
            minY = std::floor(minY / texelSizeY) * texelSizeY;
            maxY = std::floor(maxY / texelSizeY) * texelSizeY;

            glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
            glm::mat4 cascadeMatrix = lightProj * lightView;
            OutCamData.LightSpaceMatrices[cascade] = cascadeMatrix;

            m_CascadeShadowFramebuffers[cascade]->Bind();
            FRenderCommand::SetViewport(0, 0, 2048, 2048);
            FRenderCommand::Clear();

            FRenderCommand::SetDepthTesting(true);
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetCulling(true, ECullMode::Front);

            m_ShadowDepthShader->Bind();
            m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));

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
            m_CascadeShadowFramebuffers[cascade]->Unbind();
        }
    }

    void FScene::RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp,
                                      const glm::vec3& InSpotLightPos,
                                      FCameraBufferData& OutCamData) {
        if (!InSpotLightComp || !InSpotLightComp->bEnabled || !m_SpotShadowFramebuffer || !m_ShadowDepthShader)
            return;

        glm::vec3 spotPos = InSpotLightPos;
        glm::vec3 spotDir = glm::normalize(InSpotLightComp->Light.Direction);
        glm::vec3 up = (std::abs(spotDir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::mat4 spotView = glm::lookAt(spotPos, spotPos + spotDir, up);

        float fov = InSpotLightComp->Light.OuterCutOff * 2.0f;
        fov = glm::clamp(fov, 10.0f, 160.0f);
        glm::mat4 spotProj = glm::perspective(glm::radians(fov), 1.0f, 0.1f, 35.0f);
        glm::mat4 spotLightSpace = spotProj * spotView;

        OutCamData.SpotLightSpaceMatrix = spotLightSpace;

        m_SpotShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, 1024, 1024);
        FRenderCommand::Clear();

        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Front);

        m_ShadowDepthShader->Bind();
        m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(spotLightSpace));

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
        m_SpotShadowFramebuffer->Unbind();
    }

    void FScene::RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera,
                                           const FSkyboxComponent* InSkybox,
                                           bool bHasDirLight,
                                           const FDirectionalLight& InDirLight) {
        if (!m_PlanarReflectionFramebuffer)
            return;

        uint32_t vpWidth = m_ViewportWidth > 0 ? m_ViewportWidth : 1280;
        uint32_t vpHeight = m_ViewportHeight > 0 ? m_ViewportHeight : 720;

        glm::vec3 camPos = InCamera.GetPosition();
        float pitch = InCamera.GetPitch();
        float yaw = InCamera.GetYaw();

        FPerspectiveCamera mirroredCamera = InCamera;
        mirroredCamera.SetPosition(glm::vec3(camPos.x, -camPos.y, camPos.z));
        mirroredCamera.SetRotation(-pitch, yaw);

        // Lengyel's Oblique Near-Plane Clipping Frustum for arbitrary horizontal plane (Y = 0)
        glm::vec4 clipPlaneWorld(0.0f, 1.0f, 0.0f, 0.0f);
        glm::mat4 mirrorView = mirroredCamera.GetViewMatrix();
        glm::mat4 mirrorProj = mirroredCamera.GetProjectionMatrix();

        glm::vec4 clipPlaneCamera = glm::transpose(glm::inverse(mirrorView)) * clipPlaneWorld;
        glm::vec4 q((clipPlaneCamera.x > 0.0f ? 1.0f : -1.0f), (clipPlaneCamera.y > 0.0f ? 1.0f : -1.0f), 1.0f, 1.0f);
        q = glm::inverse(mirrorProj) * q;

        glm::vec4 c = clipPlaneCamera * (2.0f / glm::dot(clipPlaneCamera, q));
        mirrorProj[0][2] = c.x - mirrorProj[0][3];
        mirrorProj[1][2] = c.y - mirrorProj[1][3];
        mirrorProj[2][2] = c.z - mirrorProj[2][3];
        mirrorProj[3][2] = c.w - mirrorProj[3][3];

        glm::mat4 mirroredObliqueVP = mirrorProj * mirrorView;

        if (m_CameraUBO) {
            FCameraBufferData mirrorCamData;
            mirrorCamData.ViewProjection = mirroredObliqueVP;
            mirrorCamData.CameraPosition = glm::vec4(mirroredCamera.GetPosition(), 1.0f);
            m_CameraUBO->SetData(&mirrorCamData, sizeof(FCameraBufferData), 0);
        }

        m_PlanarReflectionFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);

        // 1. Render Skybox into reflection
        if (InSkybox && InSkybox->bEnabled && m_SkyboxShader && m_SkyboxVA) {
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetDepthMask(false);

            m_SkyboxShader->Bind();
            m_SkyboxShader->SetMat4("u_View", glm::value_ptr(mirroredCamera.GetViewMatrix()));
            m_SkyboxShader->SetMat4("u_Projection", glm::value_ptr(mirroredCamera.GetProjectionMatrix()));

            if (InSkybox->bUseHDREnvironmentMap && InSkybox->HDREnvironmentMap) {
                InSkybox->HDREnvironmentMap->Bind(0);
                m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 1);
                m_SkyboxShader->SetInt("u_HDREnvironmentMap", 0);
            } else {
                m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 0);
                glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.0f, 1.0f, 0.0f);
                m_SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
                m_SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g,
                                          InSkybox->SkyZenithColor.b);
                m_SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g,
                                          InSkybox->HorizonColor.b);
                m_SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g,
                                          InSkybox->GroundColor.b);
                m_SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g,
                                          InSkybox->SunColor.b);
                m_SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
            }
            m_SkyboxShader->SetFloat("u_Exposure", InSkybox->Exposure);

            m_SkyboxVA->Bind();
            FRenderCommand::DrawIndexed(m_SkyboxVA);

            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        }

        // 2. Render visible objects into reflection
        auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisibleInReflection)
                continue;

            mesh.Shader->Bind();
            mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            mesh.Shader->SetInt("u_UseShadows", 0);
            mesh.Shader->SetInt("u_UseSpotShadows", 0);

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

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // 3. Render 3D Text Actors into reflection
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

    void FScene::RenderGeometryPass(const FPerspectiveCamera& InCamera,
                                    bool bHasDirLight,
                                    bool bHasSpotLight,
                                    uint32_t InVpWidth,
                                    uint32_t InVpHeight) {
        auto meshView = m_Registry.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader)
                continue;

            mesh.Shader->Bind();

            // Cascaded Directional Shadow Maps (Slots 10, 11, 12)
            if (bHasDirLight && mesh.bReceiveShadows && m_CascadeShadowFramebuffers[0]) {
                m_CascadeShadowFramebuffers[0]->BindDepthTexture(10);
                m_CascadeShadowFramebuffers[1]->BindDepthTexture(11);
                m_CascadeShadowFramebuffers[2]->BindDepthTexture(12);
                mesh.Shader->SetInt("u_ShadowMap0", 10);
                mesh.Shader->SetInt("u_ShadowMap1", 11);
                mesh.Shader->SetInt("u_ShadowMap2", 12);
                mesh.Shader->SetInt("u_UseShadows", 1);
            } else {
                mesh.Shader->SetInt("u_UseShadows", 0);
            }

            // Spot Light Shadow Map (Slot 13)
            if (bHasSpotLight && mesh.bReceiveShadows && m_SpotShadowFramebuffer) {
                m_SpotShadowFramebuffer->BindDepthTexture(13);
                mesh.Shader->SetInt("u_SpotShadowMap", 13);
                mesh.Shader->SetInt("u_UseSpotShadows", 1);
            } else {
                mesh.Shader->SetInt("u_UseSpotShadows", 0);
            }

            // Planar Ground Reflection Map (Slot 5)
            bool bApplyPlanarReflection = false;
            if (m_Registry.all_of<FPBRMaterialComponent>(entity)) {
                const auto& pbrMat = m_Registry.get<FPBRMaterialComponent>(entity).Material;
                if (pbrMat.bUsePlanarReflection && m_PlanarReflectionFramebuffer) {
                    bApplyPlanarReflection = true;
                }
            }

            if (bApplyPlanarReflection) {
                m_PlanarReflectionFramebuffer->BindTexture(0, 5);
                mesh.Shader->SetInt("u_PlanarReflectionMap", 5);
                mesh.Shader->SetInt("u_UsePlanarReflection", 1);
                mesh.Shader->SetFloat2("u_ScreenSize", static_cast<float>(InVpWidth), static_cast<float>(InVpHeight));
            } else {
                mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            }

            // PBR Material Configuration
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

                // Image-Based Lighting (IBL) Environment (Slots 6, 7, 8)
                if (m_bUseIBL && m_IBLEnvironment.BRDFLUT) {
                    mesh.Shader->SetInt("u_UseIBL", 1);
                    if (m_IBLEnvironment.BRDFLUT) {
                        m_IBLEnvironment.BRDFLUT->Bind(6);
                        mesh.Shader->SetInt("u_BRDFLUT", 6);
                    }
                    if (m_IBLEnvironment.IrradianceMap) {
                        m_IBLEnvironment.IrradianceMap->Bind(7);
                        mesh.Shader->SetInt("u_IrradianceMap", 7);
                    }
                    if (m_IBLEnvironment.PrefilterMap) {
                        m_IBLEnvironment.PrefilterMap->Bind(8);
                        mesh.Shader->SetInt("u_PrefilterMap", 8);
                    }
                } else {
                    mesh.Shader->SetInt("u_UseIBL", 0);
                }
            }

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }
    }

    void FScene::RenderSkyboxPass(const FPerspectiveCamera& InCamera,
                                  const FSkyboxComponent* InSkybox,
                                  bool bHasDirLight,
                                  const FDirectionalLight& InDirLight) {
        if (!InSkybox || !InSkybox->bEnabled || !m_SkyboxShader || !m_SkyboxVA)
            return;

        FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
        FRenderCommand::SetDepthMask(false);

        m_SkyboxShader->Bind();
        m_SkyboxShader->SetMat4("u_View", glm::value_ptr(InCamera.GetViewMatrix()));
        m_SkyboxShader->SetMat4("u_Projection", glm::value_ptr(InCamera.GetProjectionMatrix()));

        if (InSkybox->bUseHDREnvironmentMap && InSkybox->HDREnvironmentMap) {
            InSkybox->HDREnvironmentMap->Bind(0);
            m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 1);
            m_SkyboxShader->SetInt("u_HDREnvironmentMap", 0);
        } else {
            m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 0);
            glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.0f, 1.0f, 0.0f);
            m_SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
            m_SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g,
                                      InSkybox->SkyZenithColor.b);
            m_SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g,
                                      InSkybox->HorizonColor.b);
            m_SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g,
                                      InSkybox->GroundColor.b);
            m_SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g,
                                      InSkybox->SunColor.b);
            m_SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
        }
        m_SkyboxShader->SetFloat("u_Exposure", InSkybox->Exposure);

        m_SkyboxVA->Bind();
        FRenderCommand::DrawIndexed(m_SkyboxVA);

        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
    }

    void FScene::RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight) {
        if (!m_HDRSceneFramebuffer || !m_PostProcessShader || !m_FullscreenQuadVA)
            return;

        FRenderCommand::BindFramebuffer(InTargetFBO);
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);
        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);

        m_PostProcessShader->Bind();
        m_HDRSceneFramebuffer->BindTexture(0, 0);
        m_PostProcessShader->SetInt("u_SceneTexture", 0);
        m_PostProcessShader->SetFloat("u_Exposure", InExposure);

        m_FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(m_FullscreenQuadVA);

        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
    }

    void FScene::OnRender(const FPerspectiveCamera& InCamera) {
        uint32_t previousFBO = FRenderCommand::GetFramebufferBinding();
        uint32_t vpWidth = m_ViewportWidth > 0 ? m_ViewportWidth : 1280;
        uint32_t vpHeight = m_ViewportHeight > 0 ? m_ViewportHeight : 720;

        // 1. Gather Light Sources and Environment Settings
        bool bHasDirLight = false;
        FDirectionalLightComponent dirLightComp;
        {
            auto dirLightView = m_Registry.view<FDirectionalLightComponent>();
            for (auto entity : dirLightView) {
                const auto& comp = dirLightView.get<FDirectionalLightComponent>(entity);
                if (comp.bEnabled) {
                    dirLightComp = comp;
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

        bool bHasSpotLight = false;
        FSpotLightComponent firstSpotLightComp;
        glm::vec3 firstSpotLightPos{0.0f};
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
                    if (!bHasSpotLight) {
                        firstSpotLightComp = comp;
                        firstSpotLightPos = sl.Position;
                        bHasSpotLight = true;
                    }
                    spotLights.push_back(sl);
                }
            }
        }

        FSkyboxComponent skybox;
        bool bHasSkybox = false;
        {
            auto skyboxView = m_Registry.view<FSkyboxComponent>();
            for (auto entity : skyboxView) {
                skybox = skyboxView.get<FSkyboxComponent>(entity);
                bHasSkybox = true;
                break;
            }
        }

        // Dynamically update real IBL when Skybox / HDR is modified
        if (bHasSkybox) {
            UpdateIBL(skybox);
        }

        // 2. Upload Scene Lighting Data into std140 Lighting UBO (Binding 1)
        FLightingBufferData lightingData;
        if (bHasDirLight) {
            lightingData.DirLight.Direction = glm::vec4(dirLightComp.Light.Direction, 1.0f);
            lightingData.DirLight.Color = glm::vec4(dirLightComp.Light.Color, dirLightComp.Light.AmbientIntensity);
            lightingData.DirLight.Intensities =
                glm::vec4(dirLightComp.Light.DiffuseIntensity, dirLightComp.Light.SpecularIntensity, 0.0f, 0.0f);
        } else {
            lightingData.DirLight.Direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        }

        for (size_t i = 0; i < pointLights.size(); ++i) {
            lightingData.PointLights[i].Position = glm::vec4(pointLights[i].Position, 1.0f);
            lightingData.PointLights[i].Color = glm::vec4(pointLights[i].Color, pointLights[i].AmbientIntensity);
            lightingData.PointLights[i].Attenuation =
                glm::vec4(pointLights[i].Constant, pointLights[i].Linear, pointLights[i].Quadratic,
                          pointLights[i].DiffuseIntensity);
            lightingData.PointLights[i].Params = glm::vec4(pointLights[i].SpecularIntensity, 0.0f, 0.0f, 0.0f);
        }

        for (size_t i = 0; i < spotLights.size(); ++i) {
            lightingData.SpotLights[i].Position = glm::vec4(spotLights[i].Position, 1.0f);
            lightingData.SpotLights[i].Direction =
                glm::vec4(spotLights[i].Direction, std::cos(glm::radians(spotLights[i].CutOff)));
            lightingData.SpotLights[i].Color =
                glm::vec4(spotLights[i].Color, std::cos(glm::radians(spotLights[i].OuterCutOff)));
            lightingData.SpotLights[i].Attenuation =
                glm::vec4(spotLights[i].Constant, spotLights[i].Linear, spotLights[i].Quadratic,
                          spotLights[i].DiffuseIntensity);
            lightingData.SpotLights[i].Params =
                glm::vec4(spotLights[i].AmbientIntensity, spotLights[i].SpecularIntensity, 0.0f, 0.0f);
        }

        lightingData.LightCounts =
            glm::ivec4(static_cast<int>(pointLights.size()), static_cast<int>(spotLights.size()), 0, 0);
        lightingData.EnvSkyColor = glm::vec4(skybox.SkyZenithColor, skybox.EnvironmentIntensity);
        lightingData.EnvHorizonColor = glm::vec4(skybox.HorizonColor, 0.0f);
        lightingData.EnvGroundColor = glm::vec4(skybox.GroundColor, 0.0f);

        if (m_LightingUBO) {
            m_LightingUBO->SetData(&lightingData, sizeof(FLightingBufferData), 0);
        }

        FCameraBufferData mainCamData;
        mainCamData.ViewProjection = InCamera.GetViewProjectionMatrix();
        mainCamData.CameraPosition = glm::vec4(InCamera.GetPosition(), 1.0f);

        // ==========================================
        // PASS 1: Cascaded Directional Shadow Depth Pass (3 Splits)
        // ==========================================
        if (bHasDirLight) {
            RenderCascadedShadowPass(InCamera, &dirLightComp, mainCamData);
        }

        // ==========================================
        // PASS 2: Spot Light Shadow Depth Pass
        // ==========================================
        if (bHasSpotLight) {
            RenderSpotShadowPass(&firstSpotLightComp, firstSpotLightPos, mainCamData);
        }

        // ==========================================
        // PASS 3: Real-Time Planar Reflection Pass
        // ==========================================
        RenderPlanarReflectionPass(InCamera, bHasSkybox ? &skybox : nullptr, bHasDirLight, dirLightComp.Light);

        // ==========================================
        // PASS 4: Main Scene HDR Render Pass
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

        if (m_CameraUBO) {
            m_CameraUBO->SetData(&mainCamData, sizeof(FCameraBufferData), 0);
        }

        // Render Geometry
        RenderGeometryPass(InCamera, bHasDirLight, bHasSpotLight, vpWidth, vpHeight);

        // Render Skybox
        if (bHasSkybox) {
            RenderSkyboxPass(InCamera, &skybox, bHasDirLight, dirLightComp.Light);
        }

        // Render 3D World Text
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
            // PASS 5: ACES Tonemapping Post-Process Composite
            // ==========================================
            RenderPostProcessPass(skybox.Exposure, previousFBO, vpWidth, vpHeight);
        }
    }

} // namespace Leon
