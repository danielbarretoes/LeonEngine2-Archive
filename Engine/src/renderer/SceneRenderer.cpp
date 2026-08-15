#include "renderer/SceneRenderer.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/Framebuffer.hpp"
#include "renderer/IBLGenerator.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/RenderCommand.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/TextRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"
#include "scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace Leon {

    FSceneRenderer::FSceneRenderer(FScene* InScene) : m_Scene(InScene) {
        // -----------------------------------------------------------------------
        // 1. Shadow framebuffers — DEPTH32F (Texture2DArray for CSM, 2D for Spot)
        // -----------------------------------------------------------------------
        FFramebufferSpecification csmSpec;
        csmSpec.Width  = 2048;
        csmSpec.Height = 2048;
        csmSpec.ArrayLayers = 3;
        csmSpec.Attachments = {EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW};
        m_CascadeShadowFramebuffer = FFramebuffer::Create(csmSpec);

        FFramebufferSpecification spotSpec;
        spotSpec.Width  = 1024;
        spotSpec.Height = 1024;
        spotSpec.Attachments = {EFramebufferTextureFormat::DEPTH32F_SHADOW};
        m_SpotShadowFramebuffer = FFramebuffer::Create(spotSpec);

        // -----------------------------------------------------------------------
        // 2. Offscreen framebuffers
        // -----------------------------------------------------------------------
        FFramebufferSpecification planarSpec;
        planarSpec.Width  = 1280;
        planarSpec.Height = 720;
        planarSpec.Attachments = {EFramebufferTextureFormat::RGBA8, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_PlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);

        FFramebufferSpecification hdrSpec;
        hdrSpec.Width  = 1280;
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        m_HDRSceneFramebuffer = FFramebuffer::Create(hdrSpec);

        // -----------------------------------------------------------------------
        // 3. UBOs (std140 — binding 0: camera/shadows, binding 1: lighting)
        // -----------------------------------------------------------------------
        m_CameraUBO   = FUniformBuffer::Create(sizeof(FCameraBufferData), 0);
        m_LightingUBO = FUniformBuffer::Create(sizeof(FLightingBufferData), 1);

        // -----------------------------------------------------------------------
        // 4. Pipeline shaders and geometry
        // -----------------------------------------------------------------------
        m_ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");
        m_SkyboxShader      = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");
        m_PostProcessShader = FShader::Create("Engine/Assets/Shaders/PostProcess.glsl");

        m_SkyboxVA          = FMeshPrimitives::CreateCube(2.0f);
        m_FullscreenQuadVA  = FMeshPrimitives::CreateQuad(2.0f, 2.0f);

        // -----------------------------------------------------------------------
        // 5. Fallback 1x1 textures (keeps all shader texture units valid & defined)
        // -----------------------------------------------------------------------
        m_DefaultWhiteTexture = FTexture2D::Create(1, 1);
        uint32_t whitePixel = 0xFFFFFFFF;
        m_DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

        m_DefaultBlackTexture = FTexture2D::Create(1, 1);
        uint32_t blackPixel = 0xFF000000;
        m_DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));

        m_DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
        uint32_t flatNormalPixel = 0xFFFF8080; // RGBA: (128, 128, 255, 255)
        m_DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));

        // -----------------------------------------------------------------------
        // 6. Initial IBL state (deferred until first skybox evaluation)
        // -----------------------------------------------------------------------
        m_bEnvironmentGenerated = false;
    }

    // =========================================================================
    void FSceneRenderer::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        m_ViewportWidth  = InWidth;
        m_ViewportHeight = InHeight;

        if (InWidth > 0 && InHeight > 0) {
            if (m_HDRSceneFramebuffer &&
                (m_HDRSceneFramebuffer->GetSpecification().Width  != InWidth ||
                 m_HDRSceneFramebuffer->GetSpecification().Height != InHeight)) {
                m_HDRSceneFramebuffer->Resize(InWidth, InHeight);
            }
            if (m_PlanarReflectionFramebuffer &&
                (m_PlanarReflectionFramebuffer->GetSpecification().Width  != InWidth ||
                 m_PlanarReflectionFramebuffer->GetSpecification().Height != InHeight)) {
                m_PlanarReflectionFramebuffer->Resize(InWidth, InHeight);
            }
        }
    }

    // =========================================================================
    // Main Render — frame entry point
    // =========================================================================
    void FSceneRenderer::Render(const FPerspectiveCamera& InCamera) {
        if (!m_Scene) return;

        auto& reg = m_Scene->GetRegistry();

        // Audit fix FASE-8: track FBO in CPU instead of querying with glGetIntegerv per frame
        m_PreviousFBO = FRenderCommand::GetFramebufferBinding();

        uint32_t vpWidth  = m_ViewportWidth  > 0 ? m_ViewportWidth  : 1280;
        uint32_t vpHeight = m_ViewportHeight > 0 ? m_ViewportHeight : 720;

        // ------------------------------------------------------------------
        // Gather lights
        // ------------------------------------------------------------------
        bool bHasDirLight = false;
        FDirectionalLightComponent dirLightComp;
        {
            auto view = reg.view<FDirectionalLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<FDirectionalLightComponent>(entity);
                if (comp.bEnabled) { dirLightComp = comp; bHasDirLight = true; break; }
            }
        }

        std::vector<FPointLight> pointLights;
        {
            auto view = reg.view<FPointLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<FPointLightComponent>(entity);
                if (comp.bEnabled && pointLights.size() < 16) {
                    FPointLight pl = comp.Light;
                    if (reg.all_of<FTransformComponent>(entity))
                        pl.Position = reg.get<FTransformComponent>(entity).Translation;
                    pointLights.push_back(pl);
                }
            }
        }

        bool bHasSpotLight = false;
        FSpotLightComponent firstSpotComp;
        glm::vec3 firstSpotPos{0.0f};
        std::vector<FSpotLight> spotLights;
        {
            auto view = reg.view<FSpotLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<FSpotLightComponent>(entity);
                if (comp.bEnabled && spotLights.size() < 8) {
                    FSpotLight sl = comp.Light;
                    if (reg.all_of<FTransformComponent>(entity))
                        sl.Position = reg.get<FTransformComponent>(entity).Translation;
                    if (!bHasSpotLight) {
                        firstSpotComp = comp;
                        firstSpotPos  = sl.Position;
                        bHasSpotLight = true;
                    }
                    spotLights.push_back(sl);
                }
            }
        }

        FSkyboxComponent skybox;
        bool bHasSkybox = false;
        {
            auto view = reg.view<FSkyboxComponent>();
            for (auto entity : view) { skybox = view.get<FSkyboxComponent>(entity); bHasSkybox = true; break; }
        }

        if (bHasSkybox) {
            UpdateIBL(skybox);
        } else if (!m_bEnvironmentGenerated) {
            UpdateIBL(FSkyboxComponent{});
        }

        // ------------------------------------------------------------------
        // Upload Lighting UBO (Binding 1)
        // ------------------------------------------------------------------
        FLightingBufferData lightingData;
        if (bHasDirLight) {
            lightingData.DirLight.Direction = glm::vec4(glm::normalize(dirLightComp.Light.Direction), 1.0f);
            lightingData.DirLight.Color     = glm::vec4(dirLightComp.Light.Color, dirLightComp.Light.Intensity);
        } else {
            lightingData.DirLight.Direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
            lightingData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        }

        for (size_t i = 0; i < pointLights.size(); ++i) {
            lightingData.PointLights[i].Position = glm::vec4(pointLights[i].Position, 1.0f);
            lightingData.PointLights[i].Color    = glm::vec4(pointLights[i].Color, pointLights[i].Intensity);
            lightingData.PointLights[i].Params   = glm::vec4(pointLights[i].Radius, 0.0f, 0.0f, 0.0f);
        }

        for (size_t i = 0; i < spotLights.size(); ++i) {
            lightingData.SpotLights[i].Position  = glm::vec4(spotLights[i].Position, 1.0f);
            lightingData.SpotLights[i].Direction =
                glm::vec4(glm::normalize(spotLights[i].Direction), std::cos(glm::radians(spotLights[i].CutOff)));
            lightingData.SpotLights[i].Color     =
                glm::vec4(spotLights[i].Color, std::cos(glm::radians(spotLights[i].OuterCutOff)));
            lightingData.SpotLights[i].Params    =
                glm::vec4(spotLights[i].Radius, spotLights[i].Intensity, 0.0f, 0.0f);
        }

        lightingData.LightCounts    = glm::ivec4(static_cast<int>(pointLights.size()),
                                                  static_cast<int>(spotLights.size()), 0, 0);
        lightingData.EnvSkyColor    = glm::vec4(skybox.SkyZenithColor, skybox.EnvironmentIntensity);
        lightingData.EnvHorizonColor = glm::vec4(skybox.HorizonColor, 0.0f);
        lightingData.EnvGroundColor  = glm::vec4(skybox.GroundColor, 0.0f);

        if (m_LightingUBO)
            m_LightingUBO->SetData(&lightingData, sizeof(FLightingBufferData), 0);

        // ------------------------------------------------------------------
        // Camera UBO base
        // ------------------------------------------------------------------
        FCameraBufferData mainCamData;
        mainCamData.ViewProjection  = InCamera.GetViewProjectionMatrix();
        mainCamData.CameraPosition  = glm::vec4(InCamera.GetPosition(), 1.0f);
        mainCamData.CameraForward   = glm::vec4(InCamera.GetForwardDirection(), 0.0f);

        // ------------------------------------------------------------------
        // PASS 1: Cascaded Shadow Pass
        // ------------------------------------------------------------------
        if (bHasDirLight)
            RenderCascadedShadowPass(InCamera, &dirLightComp, mainCamData);

        // PASS 2: Spot Shadow Pass
        if (bHasSpotLight)
            RenderSpotShadowPass(&firstSpotComp, firstSpotPos, mainCamData);

        // PASS 3: Planar Reflection
        RenderPlanarReflectionPass(InCamera, bHasSkybox ? &skybox : nullptr,
                                   bHasDirLight, dirLightComp.Light);

        // PASS 4: Main HDR Scene
        if (m_HDRSceneFramebuffer)
            m_HDRSceneFramebuffer->Bind();
        else
            FRenderCommand::BindFramebuffer(m_PreviousFBO);

        FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);

        FRenderer::ResetStats();

        if (m_CameraUBO)
            m_CameraUBO->SetData(&mainCamData, sizeof(FCameraBufferData), 0);

        // Geometry
        RenderGeometryPass(InCamera, bHasDirLight, bHasSpotLight, vpWidth, vpHeight);

        // Skybox
        if (bHasSkybox)
            RenderSkyboxPass(InCamera, &skybox, bHasDirLight, dirLightComp.Light);

        // 3D World Text
        auto textView = m_Scene->GetRegistry().view<FTransformComponent, FTextComponent>();
        FTextRenderer::BeginScene(InCamera);
        for (auto entity : textView) {
            auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
            if (textComp.Text.empty()) continue;
            glm::mat4 model = transform.GetTransform();
            FTextRenderer::DrawString(textComp.Text, model, textComp.Color,
                                      textComp.Size, textComp.Alignment, textComp.bDoubleSided);
        }
        FTextRenderer::EndScene();

        if (m_HDRSceneFramebuffer)
            m_HDRSceneFramebuffer->Unbind();

        // PASS 5: Post-process (ACES tonemapping + gamma)
        RenderPostProcessPass(skybox.Exposure, m_PreviousFBO, vpWidth, vpHeight);
    }

    // =========================================================================
    // PASS 1: Cascaded Shadow Pass (OpenGL 4.5 Texture2DArray)
    // =========================================================================
    void FSceneRenderer::RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                                   const FDirectionalLightComponent* InDirLightComp,
                                                   FCameraBufferData& OutCamData) {
        if (!InDirLightComp || !InDirLightComp->bEnabled || !m_ShadowDepthShader || !m_CascadeShadowFramebuffer)
            return;

        float nearClip = InCamera.GetNearClip();
        float farClip  = 75.0f;
        float split0   = 4.5f;
        float split1   = 20.0f;
        float split2   = farClip;

        OutCamData.CascadeSplits = glm::vec4(split0, split1, split2, 0.0f);

        float cascadeSplitsNear[3] = {nearClip, split0, split1};
        float cascadeSplitsFar[3]  = {split0, split1, split2};

        glm::vec3 lightDirNorm = glm::normalize(InDirLightComp->Light.Direction);

        m_CascadeShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, 2048, 2048);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Front);

        m_ShadowDepthShader->Bind();

        for (int cascade = 0; cascade < 3; ++cascade) {
            m_CascadeShadowFramebuffer->AttachDepthTextureLayer(cascade);
            FRenderCommand::Clear();

            float fov    = InCamera.GetFOV();
            float aspect = InCamera.GetAspectRatio();
            glm::mat4 subProj = glm::perspective(glm::radians(fov), aspect,
                                                  cascadeSplitsNear[cascade], cascadeSplitsFar[cascade]);
            glm::mat4 invSubVP = glm::inverse(subProj * InCamera.GetViewMatrix());

            std::vector<glm::vec4> frustumCorners;
            frustumCorners.reserve(8);
            for (unsigned int x = 0; x < 2; ++x)
                for (unsigned int y = 0; y < 2; ++y)
                    for (unsigned int z = 0; z < 2; ++z) {
                        glm::vec4 pt = invSubVP * glm::vec4(2.f*x-1.f, 2.f*y-1.f, 2.f*z-1.f, 1.f);
                        frustumCorners.push_back(pt / pt.w);
                    }

            glm::vec3 center(0.0f);
            for (const auto& v : frustumCorners) center += glm::vec3(v);
            center /= 8.0f;

            glm::vec3 lightPos  = center - lightDirNorm * 40.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.f, 1.f, 0.f));

            float minX = std::numeric_limits<float>::max(),    maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max(),    maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max(),    maxZ = std::numeric_limits<float>::lowest();

            for (const auto& v : frustumCorners) {
                glm::vec4 trf = lightView * v;
                minX = std::min(minX, trf.x); maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y); maxY = std::max(maxY, trf.y);
                minZ = std::min(minZ, trf.z); maxZ = std::max(maxZ, trf.z);
            }

            float zMargin = 30.0f;
            minZ -= zMargin;
            maxZ += zMargin;

            float worldUnitsPerTexelX = (maxX - minX) / 2048.0f;
            minX = std::floor(minX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
            maxX = minX + 2048.0f * worldUnitsPerTexelX;

            float worldUnitsPerTexelY = (maxY - minY) / 2048.0f;
            minY = std::floor(minY / worldUnitsPerTexelY) * worldUnitsPerTexelY;
            maxY = minY + 2048.0f * worldUnitsPerTexelY;

            // In OpenGL view space, camera looks along -Z.
            // Points in front of the light have negative Z in lightView.
            // zNear is distance to near plane (-maxZ), zFar is distance to far plane (-minZ).
            float nearPlane = -maxZ;
            float farPlane  = -minZ;
            if (nearPlane > farPlane) std::swap(nearPlane, farPlane);
            if (nearPlane < 0.1f) nearPlane = 0.1f;

            glm::mat4 lightProj     = glm::ortho(minX, maxX, minY, maxY, nearPlane, farPlane);
            glm::mat4 cascadeMatrix = lightProj * lightView;
            OutCamData.LightSpaceMatrices[cascade] = cascadeMatrix;

            m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));

            auto meshView = m_Scene->GetRegistry().view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray || !mesh.bCastShadows) continue;
                glm::mat4 model = transform.GetTransform();
                m_ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                mesh.VertexArray->Bind();
                FRenderCommand::DrawIndexed(mesh.VertexArray);
            }
        }

        FRenderCommand::SetCulling(false);
        m_CascadeShadowFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 2: Spot Shadow Pass
    // =========================================================================
    void FSceneRenderer::RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp,
                                               const glm::vec3& InSpotLightPos,
                                               FCameraBufferData& OutCamData) {
        if (!InSpotLightComp || !InSpotLightComp->bEnabled || !m_SpotShadowFramebuffer || !m_ShadowDepthShader)
            return;

        glm::vec3 spotDir = glm::normalize(InSpotLightComp->Light.Direction);
        glm::vec3 up = (std::abs(spotDir.y) < 0.99f) ? glm::vec3(0.f,1.f,0.f) : glm::vec3(0.f,0.f,1.f);
        glm::mat4 spotView = glm::lookAt(InSpotLightPos, InSpotLightPos + spotDir, up);

        float fov = glm::clamp(InSpotLightComp->Light.OuterCutOff * 2.0f + 2.0f, 10.0f, 160.0f);
        float farPlane = std::max(InSpotLightComp->Light.Radius * 1.05f, 1.0f);
        glm::mat4 spotProj       = glm::perspective(glm::radians(fov), 1.0f, 0.1f, farPlane);
        glm::mat4 spotLightSpace = spotProj * spotView;
        OutCamData.SpotLightSpaceMatrix = spotLightSpace;

        m_SpotShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, 1024, 1024);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);

        m_ShadowDepthShader->Bind();
        m_ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(spotLightSpace));

        auto meshView = m_Scene->GetRegistry().view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.bCastShadows) continue;
            glm::mat4 model = transform.GetTransform();
            m_ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        FRenderCommand::SetCulling(false);
        m_SpotShadowFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 3: Planar Reflection Pass
    // =========================================================================
    void FSceneRenderer::RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera,
                                                     const FSkyboxComponent* InSkybox,
                                                     bool bHasDirLight,
                                                     const FDirectionalLight& InDirLight) {
        if (!m_PlanarReflectionFramebuffer) return;

        uint32_t vpW = m_ViewportWidth  > 0 ? m_ViewportWidth  : 1280;
        uint32_t vpH = m_ViewportHeight > 0 ? m_ViewportHeight : 720;

        glm::vec3 camPos = InCamera.GetPosition();
        glm::vec3 mirrorPos = glm::vec3(camPos.x, -camPos.y, camPos.z);

        glm::mat4 reflectMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        glm::mat4 mirrorView = InCamera.GetViewMatrix() * reflectMatrix;
        glm::mat4 mirrorProj = InCamera.GetProjectionMatrix();
        glm::mat4 mirroredVP = mirrorProj * mirrorView;

        FPerspectiveCamera mirroredCamera = InCamera;
        mirroredCamera.SetPosition(mirrorPos);
        mirroredCamera.SetRotation(-InCamera.GetPitch(), InCamera.GetYaw());

        if (m_CameraUBO) {
            FCameraBufferData mirrorCamData;
            mirrorCamData.ViewProjection  = mirroredVP;
            mirrorCamData.CameraPosition  = glm::vec4(mirrorPos, 1.0f);
            mirrorCamData.CameraForward   = glm::vec4(mirroredCamera.GetForwardDirection(), 0.0f);
            m_CameraUBO->SetData(&mirrorCamData, sizeof(FCameraBufferData), 0);
        }

        m_PlanarReflectionFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, vpW, vpH);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);

        // Skybox in reflection
        if (InSkybox && InSkybox->bEnabled && m_SkyboxShader && m_SkyboxVA) {
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetDepthMask(false);
            m_SkyboxShader->Bind();
            m_SkyboxShader->SetMat4("u_View", glm::value_ptr(mirrorView));
            m_SkyboxShader->SetMat4("u_Projection", glm::value_ptr(mirrorProj));
            if (InSkybox->bUseHDREnvironmentMap && InSkybox->HDREnvironmentMap) {
                InSkybox->HDREnvironmentMap->Bind(0);
                m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 1);
                m_SkyboxShader->SetInt("u_HDREnvironmentMap", 0);
            } else {
                m_SkyboxShader->SetInt("u_UseHDREnvironmentMap", 0);
                glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.f,1.f,0.f);
                m_SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
                m_SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g, InSkybox->SkyZenithColor.b);
                m_SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g, InSkybox->HorizonColor.b);
                m_SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g, InSkybox->GroundColor.b);
                m_SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g, InSkybox->SunColor.b);
                m_SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
            }
            m_SkyboxVA->Bind();
            FRenderCommand::DrawIndexed(m_SkyboxVA);
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        }

        // Bind shadow maps to slots 10-11 so depth samplers always reference valid depth textures
        if (m_CascadeShadowFramebuffer) m_CascadeShadowFramebuffer->BindDepthTexture(10);
        if (m_SpotShadowFramebuffer)    m_SpotShadowFramebuffer->BindDepthTexture(11);

        // Bind fallback 1x1 textures on material slots
        if (m_DefaultWhiteTexture) {
            m_DefaultWhiteTexture->Bind(0);
            m_DefaultWhiteTexture->Bind(2);
            m_DefaultWhiteTexture->Bind(3);
            m_DefaultWhiteTexture->Bind(4);
        }
        if (m_DefaultFlatNormalTexture) m_DefaultFlatNormalTexture->Bind(1);
        if (m_DefaultBlackTexture)      m_DefaultBlackTexture->Bind(5);

        // Visible meshes in reflection (minimal material binding, no shadows)
        auto& reg = m_Scene->GetRegistry();
        auto meshView = reg.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisibleInReflection) continue;

            mesh.Shader->Bind();
            mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            mesh.Shader->SetInt("u_UseShadows", 0);
            mesh.Shader->SetInt("u_UseSpotShadows", 0);

            if (reg.all_of<FMaterialComponent>(entity)) {
                const auto& matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
                if (matInst)
                    matInst->Bind(mesh.Shader);
            }

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // Text in reflection
        auto textView = reg.view<FTransformComponent, FTextComponent>();
        FTextRenderer::BeginScene(mirroredCamera);
        for (auto entity : textView) {
            auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
            if (textComp.Text.empty()) continue;
            FTextRenderer::DrawString(textComp.Text, transform.GetTransform(), textComp.Color,
                                      textComp.Size, textComp.Alignment, textComp.bDoubleSided);
        }
        FTextRenderer::EndScene();

        m_PlanarReflectionFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 4: Geometry Pass
    // Audit fix ALTO-01: shadow maps + IBL bound ONCE before the loop, not per-object
    // Audit fix MEDIO-05: normal matrix calculated CPU-side, uploaded as u_NormalMatrix
    // =========================================================================
    void FSceneRenderer::RenderGeometryPass(const FPerspectiveCamera& InCamera,
                                             bool bHasDirLight,
                                             bool bHasSpotLight,
                                             uint32_t InVpWidth,
                                             uint32_t InVpHeight) {
        auto& reg = m_Scene->GetRegistry();

        // --- Bind per-frame textures ONCE (shadow maps + IBL) ---
        // Bind shadow depth maps to slots 10-11
        if (m_CascadeShadowFramebuffer) m_CascadeShadowFramebuffer->BindDepthTexture(10);
        if (m_SpotShadowFramebuffer)    m_SpotShadowFramebuffer->BindDepthTexture(11);

        bool bShadowsAvailable    = bHasDirLight  && m_CascadeShadowFramebuffer;
        bool bSpotShadowAvailable = bHasSpotLight && m_SpotShadowFramebuffer;

        // IBL maps (slots 6-8) — same for all objects
        bool bIBLAvailable = m_bUseIBL && m_IBLEnvironment.BRDFLUT;
        if (bIBLAvailable) {
            if (m_IBLEnvironment.BRDFLUT)     m_IBLEnvironment.BRDFLUT->Bind(6);
            if (m_IBLEnvironment.IrradianceMap) m_IBLEnvironment.IrradianceMap->Bind(7);
            if (m_IBLEnvironment.PrefilterMap)  m_IBLEnvironment.PrefilterMap->Bind(8);
        }

        auto meshView = reg.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader) continue;

            mesh.Shader->Bind();

            // Shadow map enablement flags (samplers are statically bound to slots 10-11)
            mesh.Shader->SetInt("u_UseShadows", (bShadowsAvailable && mesh.bReceiveShadows) ? 1 : 0);
            mesh.Shader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && mesh.bReceiveShadows) ? 1 : 0);

            // Material Instance resolution
            TRef<FMaterialInstance> matInst = nullptr;
            if (reg.all_of<FMaterialComponent>(entity)) {
                matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
            }
            if (!matInst) {
                matInst = FAssetManager::GetDefaultMaterial()->CreateInstance();
            }

            // Planar reflection (slot 5, per-material)
            bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && m_PlanarReflectionFramebuffer;
            if (bApplyPlanarReflection) {
                m_PlanarReflectionFramebuffer->BindTexture(0, 5);
                mesh.Shader->SetInt("u_UsePlanarReflection", 1);
                mesh.Shader->SetFloat2("u_ScreenSize",
                    static_cast<float>(InVpWidth), static_cast<float>(InVpHeight));
            } else {
                mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            }

            // Apply Material Pipeline State (Culling, Depth, Blend)
            const auto& pso = matInst->GetPipelineState();
            FRenderCommand::SetCulling(pso.CullMode != ECullMode::None, pso.CullMode);
            FRenderCommand::SetDepthTesting(pso.bDepthTest);
            FRenderCommand::SetDepthMask(pso.bDepthWrite);
            FRenderCommand::SetDepthFunc(pso.DepthFunc);
            FRenderCommand::SetBlendState(pso.bBlend);
            if (pso.bBlend) {
                FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
            }

            // Bind resolved material parameters and textures (slots 0..5)
            matInst->Bind(mesh.Shader);

            // IBL enablement (samplers are statically bound to slots 6-8)
            mesh.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            mesh.Shader->SetInt("u_DebugMode", m_DebugMode);

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));

            // Audit fix MEDIO-05: compute normal matrix on CPU, not in vertex shader per-vertex
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // Restore pass-level default rasterizer state
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
    }

    // =========================================================================
    // Skybox Pass
    // =========================================================================
    void FSceneRenderer::RenderSkyboxPass(const FPerspectiveCamera& InCamera,
                                           const FSkyboxComponent* InSkybox,
                                           bool bHasDirLight,
                                           const FDirectionalLight& InDirLight) {
        if (!InSkybox || !InSkybox->bEnabled || !m_SkyboxShader || !m_SkyboxVA) return;

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
            glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.f,1.f,0.f);
            m_SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
            m_SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g, InSkybox->SkyZenithColor.b);
            m_SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g, InSkybox->HorizonColor.b);
            m_SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g, InSkybox->GroundColor.b);
            m_SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g, InSkybox->SunColor.b);
            m_SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
        }

        m_SkyboxVA->Bind();
        FRenderCommand::DrawIndexed(m_SkyboxVA);

        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
    }

    // =========================================================================
    // Post-Process Pass (ACES tonemapping + gamma correction)
    // =========================================================================
    void FSceneRenderer::RenderPostProcessPass(float InExposure, uint32_t InTargetFBO,
                                                uint32_t InVpWidth, uint32_t InVpHeight) {
        if (!m_HDRSceneFramebuffer || !m_PostProcessShader || !m_FullscreenQuadVA) return;

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

    // =========================================================================
    // IBL update (triggered when HDR path changes)
    // =========================================================================
    void FSceneRenderer::UpdateIBL(const FSkyboxComponent& InSkybox) {
        std::string currentHdr = InSkybox.HDREnvironmentMapPath;
        if (currentHdr.empty() && InSkybox.HDREnvironmentMap)
            currentHdr = InSkybox.HDREnvironmentMap->GetPath();

        if (!m_bEnvironmentGenerated || (InSkybox.bUseHDREnvironmentMap && !currentHdr.empty() && currentHdr != m_LoadedHDRPath) ||
            (!InSkybox.bUseHDREnvironmentMap && !m_LoadedHDRPath.empty())) {
            m_IBLEnvironment = FIBLGenerator::CreateEnvironmentFromSkybox(InSkybox);
            m_LoadedHDRPath  = InSkybox.bUseHDREnvironmentMap ? currentHdr : "";
            m_bEnvironmentGenerated = true;
        }
    }

} // namespace Leon
