#include "Renderer/FWorldRenderer.hpp"
#include "Core/FLog.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/FLightmapAsset.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FFramebuffer.hpp"
#include "Renderer/FFrustumCull.hpp"
#include "Renderer/FIBLGenerator.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FRenderer.hpp"
#include "Renderer/FTextRenderer.hpp"
#include "RHI/FVertexArray.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>

namespace Leon {

    namespace {
        bool IsStaticMeshCulled(const FTransformComponent& InTransform, const UStaticMeshComponent& InMesh,
                                const FFrustumPlanes& InFrustum) {
            if (!InMesh.StaticMesh)
                return false;
            glm::vec3 wMin, wMax;
            TransformAABB(InMesh.StaticMesh->GetBoundsMin(), InMesh.StaticMesh->GetBoundsMax(),
                          InTransform.GetTransform(), wMin, wMax);
            if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
                FRenderer::GetStatsMutable().MeshesCulled++;
                return true;
            }
            FRenderer::GetStatsMutable().MeshesDrawn++;
            return false;
        }

        bool IsProceduralMeshCulled(const FTransformComponent& InTransform, const FMeshComponent& InMesh,
                                    const FFrustumPlanes& InFrustum) {
            float e = std::max({InMesh.MeshSize * 0.5f, InMesh.MeshRadius, InMesh.MeshWidth * 0.5f,
                                InMesh.MeshHeight * 0.5f, InMesh.MeshDepth * 0.5f, 0.5f});
            glm::vec3 localMin(-e), localMax(e);
            glm::vec3 wMin, wMax;
            TransformAABB(localMin, localMax, InTransform.GetTransform(), wMin, wMax);
            if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
                FRenderer::GetStatsMutable().MeshesCulled++;
                return true;
            }
            FRenderer::GetStatsMutable().MeshesDrawn++;
            return false;
        }

        void BindLightmapUniforms(FShader& InShader, bool bUseLightmap, bool bUseTexCoord,
                                   const glm::vec2& InScale, const glm::vec2& InBias,
                                   const TRef<FTexture2D>& InTexture) {
            if (bUseLightmap && InTexture) {
                InTexture->Bind(12);
                InShader.SetInt("u_Lightmap", 12);
                InShader.SetInt("u_UseLightmap", 1);
                InShader.SetInt("u_LightmapUseTexCoord", bUseTexCoord ? 1 : 0);
                InShader.SetFloat2("u_LightmapScale", InScale.x, InScale.y);
                InShader.SetFloat2("u_LightmapBias", InBias.x, InBias.y);
            } else {
                InShader.SetInt("u_UseLightmap", 0);
            }
        }
    } // namespace

    FWorldRenderer::FWorldRenderer(UWorld* InWorld) : World(InWorld) {
        if (InWorld && InWorld->HasPendingRendererDefaults()) {
            ShadowSettings.CascadeResolution = InWorld->GetPendingShadowMapResolution();
            bEnablePlanarReflection = InWorld->GetPendingPlanarReflectionEnabled();
        }

        // -----------------------------------------------------------------------
        // 1. Shadow framebuffers — DEPTH32F (Texture2DArray for 4-Cascade CSM, 2D for Spot)
        // -----------------------------------------------------------------------
        FFramebufferSpecification csmSpec;
        csmSpec.Width = ShadowSettings.CascadeResolution;
        csmSpec.Height = ShadowSettings.CascadeResolution;
        csmSpec.ArrayLayers = 4;
        csmSpec.Attachments = {EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW};
        CascadeShadowFramebuffer = FFramebuffer::Create(csmSpec);

        FFramebufferSpecification spotSpec;
        spotSpec.Width = ShadowSettings.SpotResolution;
        spotSpec.Height = ShadowSettings.SpotResolution;
        spotSpec.Attachments = {EFramebufferTextureFormat::DEPTH32F_SHADOW};
        SpotShadowFramebuffer = FFramebuffer::Create(spotSpec);

        // -----------------------------------------------------------------------
        // 2. Offscreen framebuffers
        // -----------------------------------------------------------------------
        FFramebufferSpecification planarSpec;
        planarSpec.Width = 1280;
        planarSpec.Height = 720;
        // Must be HDR: PBR_Lit + Skybox write linear radiance > 1. RGBA8 clamps to white blobs on mirrors/wet floors.
        // ColorMipLevels=5 → roughness * 4.0 LOD (matches prefilter) so wet floors blur emissive stamps instead of hard
        // white rectangles.
        planarSpec.ColorMipLevels = 5;
        planarSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        PlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);

        FFramebufferSpecification hdrSpec;
        hdrSpec.Width = 1280;
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        HDRSceneFramebuffer = FFramebuffer::Create(hdrSpec);

        // -----------------------------------------------------------------------
        // 3. UBOs (std140 — binding 0: camera/shadows, binding 1: lighting)
        // -----------------------------------------------------------------------
        CameraUBO = FUniformBuffer::Create(sizeof(FCameraBufferData), 0);
        LightingUBO = FUniformBuffer::Create(sizeof(FLightingBufferData), 1);

        // -----------------------------------------------------------------------
        // 4. Pipeline shaders and geometry
        // -----------------------------------------------------------------------
        ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");
        SkyboxShader = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");
        PostProcessShader = FShader::Create("Engine/Assets/Shaders/PostProcess.glsl");

        SkyboxVA = FMeshPrimitives::CreateCube(2.0f);
        FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);

        // -----------------------------------------------------------------------
        // 5. Fallback 1x1 textures (keeps all shader texture units valid & defined)
        // -----------------------------------------------------------------------
        DefaultWhiteTexture = FTexture2D::Create(1, 1);
        uint32_t whitePixel = 0xFFFFFFFF;
        DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

        DefaultBlackTexture = FTexture2D::Create(1, 1);
        uint32_t blackPixel = 0xFF000000;
        DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));

        DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
        uint32_t flatNormalPixel = 0xFFFF8080; // RGBA: (128, 128, 255, 255)
        DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));

        // -----------------------------------------------------------------------
        // 6. Post-Processing Pipeline
        // -----------------------------------------------------------------------
        PostProcessPipeline.Init();

        // -----------------------------------------------------------------------
        // 7. Initial IBL state (deferred until first skybox evaluation)
        // -----------------------------------------------------------------------
        bEnvironmentGenerated = false;
    }

    void FWorldRenderer::ApplyProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection) {
        bEnablePlanarReflection = bInEnablePlanarReflection;
        if (InShadowMapResolution == 0 || InShadowMapResolution == ShadowSettings.CascadeResolution)
            return;

        ShadowSettings.CascadeResolution = InShadowMapResolution;
        FFramebufferSpecification csmSpec;
        csmSpec.Width = ShadowSettings.CascadeResolution;
        csmSpec.Height = ShadowSettings.CascadeResolution;
        csmSpec.ArrayLayers = 4;
        csmSpec.Attachments = {EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW};
        CascadeShadowFramebuffer = FFramebuffer::Create(csmSpec);
    }

    // =========================================================================
    void FWorldRenderer::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        ViewportWidth = InWidth;
        ViewportHeight = InHeight;

        if (InWidth > 0 && InHeight > 0) {
            if (HDRSceneFramebuffer && (HDRSceneFramebuffer->GetSpecification().Width != InWidth ||
                                          HDRSceneFramebuffer->GetSpecification().Height != InHeight)) {
                HDRSceneFramebuffer->Resize(InWidth, InHeight);
            }
            if (PlanarReflectionFramebuffer &&
                (PlanarReflectionFramebuffer->GetSpecification().Width != InWidth ||
                 PlanarReflectionFramebuffer->GetSpecification().Height != InHeight)) {
                PlanarReflectionFramebuffer->Resize(InWidth, InHeight);
            }
            PostProcessPipeline.OnViewportResize(InWidth, InHeight);
        }
    }

    // =========================================================================
    // Main Render — frame entry point
    // =========================================================================
    void FWorldRenderer::Render(const FPerspectiveCamera& InCamera) {
        if (!World)
            return;

        FRenderer::GetStatsMutable().MeshesCulled = 0;
        FRenderer::GetStatsMutable().MeshesDrawn = 0;

        auto& reg = World->GetRegistry();

        // Audit fix FASE-8: track FBO in CPU instead of querying with glGetIntegerv per frame
        PreviousFBO = FRenderCommand::GetFramebufferBinding();

        uint32_t vpWidth = ViewportWidth > 0 ? ViewportWidth : 1280;
        uint32_t vpHeight = ViewportHeight > 0 ? ViewportHeight : 720;

        // ------------------------------------------------------------------
        // Gather lights
        // ------------------------------------------------------------------
        bool bHasDirLight = false;
        UDirectionalLightComponent dirLightComp;
        {
            auto view = reg.view<UDirectionalLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<UDirectionalLightComponent>(entity);
                if (comp.bEnabled && comp.Mobility != ELightMobility::Static) {
                    dirLightComp = comp;
                    bHasDirLight = true;
                    break;
                }
            }
        }

        std::vector<FPointLight> pointLights;
        {
            auto view = reg.view<UPointLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<UPointLightComponent>(entity);
                if (comp.bEnabled && comp.Mobility != ELightMobility::Static && pointLights.size() < 16) {
                    FPointLight pl = comp.Light;
                    if (reg.all_of<FTransformComponent>(entity))
                        pl.Position = reg.get<FTransformComponent>(entity).Translation;
                    pointLights.push_back(pl);
                }
            }
        }

        bool bHasSpotLight = false;
        USpotLightComponent firstSpotComp;
        glm::vec3 firstSpotPos{0.0f};
        std::vector<FSpotLight> spotLights;
        {
            auto view = reg.view<USpotLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<USpotLightComponent>(entity);
                if (comp.bEnabled && comp.Mobility != ELightMobility::Static && spotLights.size() < 8) {
                    FSpotLight sl = comp.Light;
                    if (reg.all_of<FTransformComponent>(entity))
                        sl.Position = reg.get<FTransformComponent>(entity).Translation;
                    if (!bHasSpotLight) {
                        firstSpotComp = comp;
                        firstSpotPos = sl.Position;
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
            for (auto entity : view) {
                skybox = view.get<FSkyboxComponent>(entity);
                bHasSkybox = true;
                break;
            }
        }

        if (bHasSkybox) {
            UpdateIBL(skybox);
        } else if (!bEnvironmentGenerated) {
            UpdateIBL(FSkyboxComponent{});
        }

        // ------------------------------------------------------------------
        // Upload Lighting UBO (Binding 1)
        // ------------------------------------------------------------------
        FLightingBufferData lightingData;
        if (bHasDirLight) {
            lightingData.DirLight.Direction = glm::vec4(glm::normalize(dirLightComp.Light.Direction), 1.0f);
            lightingData.DirLight.Color = glm::vec4(dirLightComp.Light.Color, dirLightComp.Light.Intensity);
        } else {
            lightingData.DirLight.Direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
            lightingData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        }

        for (size_t i = 0; i < pointLights.size(); ++i) {
            lightingData.PointLights[i].Position = glm::vec4(pointLights[i].Position, 1.0f);
            lightingData.PointLights[i].Color = glm::vec4(pointLights[i].Color, pointLights[i].Intensity);
            lightingData.PointLights[i].Params = glm::vec4(pointLights[i].Radius, 0.0f, 0.0f, 0.0f);
        }

        for (size_t i = 0; i < spotLights.size(); ++i) {
            lightingData.SpotLights[i].Position = glm::vec4(spotLights[i].Position, 1.0f);
            lightingData.SpotLights[i].Direction =
                glm::vec4(glm::normalize(spotLights[i].Direction), std::cos(glm::radians(spotLights[i].CutOff)));
            lightingData.SpotLights[i].Color =
                glm::vec4(spotLights[i].Color, std::cos(glm::radians(spotLights[i].OuterCutOff)));
            lightingData.SpotLights[i].Params = glm::vec4(spotLights[i].Radius, spotLights[i].Intensity, 0.0f, 0.0f);
        }

        lightingData.LightCounts =
            glm::ivec4(static_cast<int>(pointLights.size()), static_cast<int>(spotLights.size()), 0, 0);
        lightingData.EnvSkyColor = glm::vec4(skybox.SkyZenithColor, skybox.EnvironmentIntensity);
        lightingData.EnvHorizonColor = glm::vec4(skybox.HorizonColor, 0.0f);
        lightingData.EnvGroundColor = glm::vec4(skybox.GroundColor, 0.0f);

        if (LightingUBO)
            LightingUBO->SetData(&lightingData, sizeof(FLightingBufferData), 0);

        // ------------------------------------------------------------------
        // Camera UBO base
        // ------------------------------------------------------------------
        FCameraBufferData mainCamData;
        mainCamData.ViewProjection = InCamera.GetViewProjectionMatrix();
        mainCamData.CameraPosition = glm::vec4(InCamera.GetPosition(), 1.0f);
        mainCamData.CameraForward = glm::vec4(InCamera.GetForwardDirection(), 0.0f);

        // ------------------------------------------------------------------
        // PASS 1: Cascaded Shadow Pass
        // ------------------------------------------------------------------
        if (bHasDirLight)
            RenderCascadedShadowPass(InCamera, &dirLightComp, mainCamData);

        // PASS 2: Spot Shadow Pass
        if (bHasSpotLight)
            RenderSpotShadowPass(&firstSpotComp, firstSpotPos, mainCamData);

        // PASS 3: Planar Reflection
        RenderPlanarReflectionPass(InCamera, bHasSkybox ? &skybox : nullptr, bHasDirLight, dirLightComp.Light);

        // PASS 4: Main HDR Scene
        if (HDRSceneFramebuffer)
            HDRSceneFramebuffer->Bind();
        else
            FRenderCommand::BindFramebuffer(PreviousFBO);

        FRenderCommand::SetViewport(0, 0, vpWidth, vpHeight);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);

        FRenderer::ResetStats();

        if (CameraUBO)
            CameraUBO->SetData(&mainCamData, sizeof(FCameraBufferData), 0);

        // Wireframe applies only to 3D scene content — never to post-process / UI / overlays
        if (bWireframeEnabled) {
            FRenderCommand::SetWireframe(true);
        }

        // Geometry
        RenderGeometryPass(InCamera, bHasDirLight, bHasSpotLight, vpWidth, vpHeight);

        // Skybox
        if (bHasSkybox)
            RenderSkyboxPass(InCamera, &skybox, bHasDirLight, dirLightComp.Light);

        // 3D World Text
        auto textView = World->GetRegistry().view<FTransformComponent, FTextComponent>();
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

        FRenderCommand::SetWireframe(false);

        if (HDRSceneFramebuffer)
            HDRSceneFramebuffer->Unbind();

        // PASS 5: Post-process (ACES tonemapping + gamma)
        RenderPostProcessPass(skybox.Exposure, PreviousFBO, vpWidth, vpHeight);
    }

    // =========================================================================
    // PASS 1: Cascaded Shadow Pass (OpenGL 4.5 Texture2DArray)
    // =========================================================================
    void FWorldRenderer::RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                                  const UDirectionalLightComponent* InDirLightComp,
                                                  FCameraBufferData& OutCamData) {
        if (!InDirLightComp || !InDirLightComp->bEnabled || !ShadowDepthShader || !CascadeShadowFramebuffer)
            return;

        float nearClip = InCamera.GetNearClip();
        float farClip = ShadowSettings.ShadowDistance;

        auto splits = ShadowMath::CalculateCascadeSplits(ShadowSettings.CascadeCount, nearClip, farClip,
                                                         ShadowSettings.SplitLambda, ShadowSettings.SplitScheme);

        OutCamData.CascadeSplits = glm::vec4(splits[1], splits[2], splits[3], splits[4]);
        OutCamData.ShadowParams = glm::vec4(ShadowSettings.ConstantBias, ShadowSettings.SlopeBias,
                                            ShadowSettings.NormalBias, ShadowSettings.CascadeBlendWidth);
        OutCamData.ShadowSettings =
            glm::ivec4(static_cast<int>(ShadowSettings.FilterMode), ShadowSettings.ContactShadowSteps,
                       ShadowSettings.bEnableContactShadows ? 1 : 0, DebugMode);
        OutCamData.ContactShadowParams =
            glm::vec4(ShadowSettings.ContactShadowDistance, ShadowSettings.ContactShadowThickness, 0.0f, 0.0f);

        CascadeShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, ShadowSettings.CascadeResolution, ShadowSettings.CascadeResolution);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Front);

        ShadowDepthShader->Bind();

        float fov = InCamera.GetFOV();
        float aspect = InCamera.GetAspectRatio();

        for (uint32_t cascade = 0; cascade < ShadowSettings.CascadeCount && cascade < 4; ++cascade) {
            CascadeShadowFramebuffer->AttachDepthTextureLayer(cascade);
            FRenderCommand::Clear();

            glm::mat4 subProj = glm::perspective(glm::radians(fov), aspect, splits[cascade], splits[cascade + 1]);
            auto corners = ShadowMath::GetFrustumCornersWorldSpace(subProj, InCamera.GetViewMatrix());

            float worldUnitsPerTexel = 0.01f;
            glm::mat4 cascadeMatrix = ShadowMath::CalculateCascadeMatrix(
                corners, InDirLightComp->Light.Direction, ShadowSettings.CascadeResolution,
                ShadowSettings.bStabilizeCascades, worldUnitsPerTexel);
            OutCamData.LightSpaceMatrices[cascade] = cascadeMatrix;
            OutCamData.CascadeOffsets[cascade] = ShadowMath::GetAtlasScaleOffset2x2(cascade);

            ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));

            auto meshView = World->GetRegistry().view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray || !mesh.bCastShadows)
                    continue;

                glm::mat4 model = transform.GetTransform();
                ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));

                // Support alpha-masked shadow casters
                if (World->GetRegistry().all_of<FMaterialComponent>(entity)) {
                    const auto& matComp = World->GetRegistry().get<FMaterialComponent>(entity);
                    if (matComp.MaterialInstance) {
                        auto alphaMode = matComp.MaterialInstance->GetAlphaMode();
                        auto albedoTex = matComp.MaterialInstance->GetTexture(0);
                        glm::vec2 tiling = matComp.MaterialInstance->GetUVTiling();
                        glm::vec2 offset = matComp.MaterialInstance->GetUVOffset();

                        ShadowDepthShader->SetInt("u_AlphaMode", static_cast<int>(alphaMode));
                        ShadowDepthShader->SetFloat("u_AlphaCutoff", matComp.MaterialInstance->GetAlphaCutoff());
                        ShadowDepthShader->SetInt("u_UseAlbedoMap", albedoTex ? 1 : 0);
                        ShadowDepthShader->SetFloat2("u_UVTiling", tiling.x, tiling.y);
                        ShadowDepthShader->SetFloat2("u_UVOffset", offset.x, offset.y);
                        if (albedoTex) {
                            albedoTex->Bind(0);
                        }
                    } else {
                        ShadowDepthShader->SetInt("u_AlphaMode", 0);
                        ShadowDepthShader->SetInt("u_UseAlbedoMap", 0);
                    }
                } else {
                    ShadowDepthShader->SetInt("u_AlphaMode", 0);
                    ShadowDepthShader->SetInt("u_UseAlbedoMap", 0);
                }

                mesh.VertexArray->Bind();
                FRenderCommand::DrawIndexed(mesh.VertexArray);
            }

            // Static Mesh Entities in CSM
            auto staticMeshView = World->GetRegistry().view<FTransformComponent, UStaticMeshComponent>();
            for (auto entity : staticMeshView) {
                auto [transform, staticMeshComp] =
                    staticMeshView.get<FTransformComponent, UStaticMeshComponent>(entity);
                if (!staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray() ||
                    !staticMeshComp.bCastShadows)
                    continue;

                staticMeshComp.StaticMesh->GetVertexArray()->Bind();
                for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                    if (submesh.IndexCount == 0)
                        continue;
                    glm::mat4 model = transform.GetTransform() * submesh.LocalTransform;
                    ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                    ShadowDepthShader->SetInt("u_AlphaMode", 0);
                    ShadowDepthShader->SetInt("u_UseAlbedoMap", 0);
                    FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                      submesh.IndexOffset);
                }
            }
        }

        FRenderCommand::SetCulling(false);
        CascadeShadowFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 2: Spot Shadow Pass
    // =========================================================================
    void FWorldRenderer::RenderSpotShadowPass(const USpotLightComponent* InSpotLightComp,
                                              const glm::vec3& InSpotLightPos, FCameraBufferData& OutCamData) {
        if (!InSpotLightComp || !InSpotLightComp->bEnabled || !SpotShadowFramebuffer || !ShadowDepthShader)
            return;

        glm::vec3 spotDir = glm::normalize(InSpotLightComp->Light.Direction);
        glm::vec3 up = (std::abs(spotDir.y) < 0.99f) ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, 0.f, 1.f);
        glm::mat4 spotView = glm::lookAt(InSpotLightPos, InSpotLightPos + spotDir, up);

        float fov = glm::clamp(InSpotLightComp->Light.OuterCutOff * 2.0f + 2.0f, 10.0f, 160.0f);
        float farPlane = std::max(InSpotLightComp->Light.Radius * 1.05f, 1.0f);
        glm::mat4 spotProj = glm::perspective(glm::radians(fov), 1.0f, 0.1f, farPlane);
        glm::mat4 spotLightSpace = spotProj * spotView;
        OutCamData.SpotLightSpaceMatrix = spotLightSpace;

        SpotShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, 1024, 1024);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);

        ShadowDepthShader->Bind();
        ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(spotLightSpace));

        auto meshView = World->GetRegistry().view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.bCastShadows)
                continue;
            glm::mat4 model = transform.GetTransform();
            ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        auto staticMeshView = World->GetRegistry().view<FTransformComponent, UStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, UStaticMeshComponent>(entity);
            if (!staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray() ||
                !staticMeshComp.bCastShadows)
                continue;
            staticMeshComp.StaticMesh->GetVertexArray()->Bind();
            for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;
                glm::mat4 model = transform.GetTransform() * submesh.LocalTransform;
                ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        FRenderCommand::SetCulling(false);
        SpotShadowFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 3: Planar Reflection Pass
    // =========================================================================
    void FWorldRenderer::RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera,
                                                     const FSkyboxComponent* InSkybox, bool bHasDirLight,
                                                     const FDirectionalLight& InDirLight) {
        if (!bEnablePlanarReflection || !PlanarReflectionFramebuffer)
            return;

        uint32_t vpW = ViewportWidth > 0 ? ViewportWidth : 1280;
        uint32_t vpH = ViewportHeight > 0 ? ViewportHeight : 720;

        glm::vec3 camPos = InCamera.GetPosition();
        glm::vec3 mirrorPos = glm::vec3(camPos.x, -camPos.y, camPos.z);

        glm::mat4 reflectMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        glm::mat4 mirrorView = InCamera.GetViewMatrix() * reflectMatrix;
        glm::mat4 mirrorProj = InCamera.GetProjectionMatrix();
        glm::mat4 mirroredVP = mirrorProj * mirrorView;

        FPerspectiveCamera mirroredCamera = InCamera;
        mirroredCamera.SetPosition(mirrorPos);
        mirroredCamera.SetRotation(-InCamera.GetPitch(), InCamera.GetYaw());

        if (CameraUBO) {
            FCameraBufferData mirrorCamData;
            mirrorCamData.ViewProjection = mirroredVP;
            mirrorCamData.CameraPosition = glm::vec4(mirrorPos, 1.0f);
            mirrorCamData.CameraForward = glm::vec4(mirroredCamera.GetForwardDirection(), 0.0f);
            CameraUBO->SetData(&mirrorCamData, sizeof(FCameraBufferData), 0);
        }

        PlanarReflectionFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, vpW, vpH);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);

        // Skybox in reflection
        if (InSkybox && InSkybox->bEnabled && SkyboxShader && SkyboxVA) {
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetDepthMask(false);
            SkyboxShader->Bind();
            SkyboxShader->SetMat4("u_View", glm::value_ptr(mirrorView));
            SkyboxShader->SetMat4("u_Projection", glm::value_ptr(mirrorProj));
            if (InSkybox->bUseHDREnvironmentMap && InSkybox->HDREnvironmentMap) {
                InSkybox->HDREnvironmentMap->Bind(0);
                SkyboxShader->SetInt("u_UseHDREnvironmentMap", 1);
                SkyboxShader->SetInt("u_HDREnvironmentMap", 0);
            } else {
                SkyboxShader->SetInt("u_UseHDREnvironmentMap", 0);
                glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.f, 1.f, 0.f);
                SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
                SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g,
                                          InSkybox->SkyZenithColor.b);
                SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g,
                                          InSkybox->HorizonColor.b);
                SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g,
                                          InSkybox->GroundColor.b);
                SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g,
                                          InSkybox->SunColor.b);
                SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
            }
            SkyboxVA->Bind();
            FRenderCommand::DrawIndexed(SkyboxVA);
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        }

        // Bind shadow maps to slots 10-11 so depth samplers always reference valid depth textures
        if (CascadeShadowFramebuffer)
            CascadeShadowFramebuffer->BindDepthTexture(10);
        if (SpotShadowFramebuffer)
            SpotShadowFramebuffer->BindDepthTexture(11);

        // Bind fallback 1x1 textures on material slots (overridden per-draw by MaterialInstance::Bind)
        if (DefaultWhiteTexture) {
            DefaultWhiteTexture->Bind(0);
            DefaultWhiteTexture->Bind(2);
            DefaultWhiteTexture->Bind(3);
            DefaultWhiteTexture->Bind(4);
        }
        if (DefaultFlatNormalTexture)
            DefaultFlatNormalTexture->Bind(1);
        if (DefaultBlackTexture)
            DefaultBlackTexture->Bind(5);

        // IBL for reflected geometry (same lighting environment as the main view)
        bool bIBLAvailable = bUseIBL && IBLEnvironment.BRDFLUT;
        if (bIBLAvailable) {
            if (IBLEnvironment.BRDFLUT)
                IBLEnvironment.BRDFLUT->Bind(6);
            if (IBLEnvironment.IrradianceMap)
                IBLEnvironment.IrradianceMap->Bind(7);
            if (IBLEnvironment.PrefilterMap)
                IBLEnvironment.PrefilterMap->Bind(8);
        }

        // Y-reflection reverses winding and must flip world normals (det < 0).
        auto CullModeForReflection = [](ECullMode InMode) -> ECullMode {
            if (InMode == ECullMode::Back)
                return ECullMode::Front;
            if (InMode == ECullMode::Front)
                return ECullMode::Back;
            return InMode;
        };
        auto NormalMatrixForReflection = [&reflectMatrix](const glm::mat4& InModel) -> glm::mat3 {
            return glm::transpose(glm::inverse(glm::mat3(reflectMatrix * InModel)));
        };

        // Visible meshes in reflection (full materials + textures; no nested planar, no shadows)
        auto& reg = World->GetRegistry();
        const FFrustumPlanes reflectionFrustum = ExtractFrustumPlanes(mirroredCamera.GetViewProjectionMatrix());
        auto meshView = reg.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisibleInReflection)
                continue;
            if (IsProceduralMeshCulled(transform, mesh, reflectionFrustum))
                continue;

            mesh.Shader->Bind();
            mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            mesh.Shader->SetInt("u_UseShadows", 0);
            mesh.Shader->SetInt("u_UseSpotShadows", 0);
            mesh.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            mesh.Shader->SetInt("u_DebugMode", 0);

            TRef<FMaterialInstance> matInst = nullptr;
            if (reg.all_of<FMaterialComponent>(entity)) {
                matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
            }
            if (!matInst) {
                matInst = UAssetManager::GetDefaultMaterial()->CreateInstance();
            }

            const auto& pso = matInst->GetPipelineState();
            ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : CullModeForReflection(pso.CullMode);
            FRenderCommand::SetCulling(cullMode != ECullMode::None, cullMode);
            FRenderCommand::SetDepthTesting(pso.bDepthTest);
            bool bDepthWrite = (matInst->GetAlphaMode() == EAlphaMode::Blend) ? false : pso.bDepthWrite;
            FRenderCommand::SetDepthMask(bDepthWrite);
            FRenderCommand::SetDepthFunc(pso.DepthFunc);
            bool bBlend = (matInst->GetAlphaMode() == EAlphaMode::Blend) || pso.bBlend;
            FRenderCommand::SetBlendState(bBlend);
            if (bBlend) {
                FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
            }

            matInst->Bind(mesh.Shader);

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            glm::mat3 normalMatrix = NormalMatrixForReflection(model);
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        auto staticMeshView = reg.view<FTransformComponent, UStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, UStaticMeshComponent>(entity);
            if (!staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray() ||
                !staticMeshComp.bVisibleInReflection)
                continue;
            if (IsStaticMeshCulled(transform, staticMeshComp, reflectionFrustum))
                continue;

            TRef<FShader> shader = staticMeshComp.Shader
                                       ? staticMeshComp.Shader
                                       : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!shader)
                continue;

            shader->Bind();
            shader->SetInt("u_UsePlanarReflection", 0);
            shader->SetInt("u_UseShadows", 0);
            shader->SetInt("u_UseSpotShadows", 0);
            shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            shader->SetInt("u_DebugMode", 0);

            staticMeshComp.StaticMesh->GetVertexArray()->Bind();
            for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;

                TRef<FMaterialInstance> matInst =
                    ResolveStaticSubmeshMaterial(*staticMeshComp.StaticMesh, submesh, staticMeshComp.MaterialOverrides);

                const auto& pso = matInst->GetPipelineState();
                ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : CullModeForReflection(pso.CullMode);
                FRenderCommand::SetCulling(cullMode != ECullMode::None, cullMode);
                FRenderCommand::SetDepthTesting(pso.bDepthTest);
                bool bDepthWrite = (matInst->GetAlphaMode() == EAlphaMode::Blend) ? false : pso.bDepthWrite;
                FRenderCommand::SetDepthMask(bDepthWrite);
                FRenderCommand::SetDepthFunc(pso.DepthFunc);
                bool bBlend = (matInst->GetAlphaMode() == EAlphaMode::Blend) || pso.bBlend;
                FRenderCommand::SetBlendState(bBlend);
                if (bBlend) {
                    FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
                }

                matInst->Bind(shader);

                glm::mat4 model = transform.GetTransform() * submesh.LocalTransform;
                shader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = NormalMatrixForReflection(model);
                shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);

        // Text in reflection
        auto textView = reg.view<FTransformComponent, FTextComponent>();
        FTextRenderer::BeginScene(mirroredCamera);
        for (auto entity : textView) {
            auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
            if (textComp.Text.empty())
                continue;
            FTextRenderer::DrawString(textComp.Text, transform.GetTransform(), textComp.Color, textComp.Size,
                                      textComp.Alignment, textComp.bDoubleSided);
        }
        FTextRenderer::EndScene();

        // Roughness-aware specular needs a planar mip chain (sampled via textureLod in PBR_Lit).
        PlanarReflectionFramebuffer->GenerateColorMipmaps();
        PlanarReflectionFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 4: Geometry Pass
    // Audit fix ALTO-01: shadow maps + IBL bound ONCE before the loop, not per-object
    // Audit fix MEDIO-05: normal matrix calculated CPU-side, uploaded as u_NormalMatrix
    // =========================================================================
    void FWorldRenderer::RenderGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight, bool bHasSpotLight,
                                            uint32_t InVpWidth, uint32_t InVpHeight) {
        auto& reg = World->GetRegistry();

        // --- Bind per-frame textures ONCE (shadow maps + IBL) ---
        // Bind shadow depth maps to slots 10-11
        if (CascadeShadowFramebuffer)
            CascadeShadowFramebuffer->BindDepthTexture(10);
        if (SpotShadowFramebuffer)
            SpotShadowFramebuffer->BindDepthTexture(11);

        bool bShadowsAvailable = bHasDirLight && CascadeShadowFramebuffer;
        bool bSpotShadowAvailable = bHasSpotLight && SpotShadowFramebuffer;

        // IBL maps (slots 6-8) — same for all objects
        bool bIBLAvailable = bUseIBL && IBLEnvironment.BRDFLUT;
        if (bIBLAvailable) {
            if (IBLEnvironment.BRDFLUT)
                IBLEnvironment.BRDFLUT->Bind(6);
            if (IBLEnvironment.IrradianceMap)
                IBLEnvironment.IrradianceMap->Bind(7);
            if (IBLEnvironment.PrefilterMap)
                IBLEnvironment.PrefilterMap->Bind(8);
        }

        auto meshView = reg.view<FTransformComponent, FMeshComponent>();
        const FFrustumPlanes camFrustum = ExtractFrustumPlanes(InCamera.GetViewProjectionMatrix());
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader)
                continue;

            if (IsProceduralMeshCulled(transform, mesh, camFrustum))
                continue;

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
                matInst = UAssetManager::GetDefaultMaterial()->CreateInstance();
            }

            // Planar reflection (slot 5, per-material)
            bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
            if (bApplyPlanarReflection) {
                PlanarReflectionFramebuffer->BindTexture(0, 5);
                mesh.Shader->SetInt("u_UsePlanarReflection", 1);
                mesh.Shader->SetFloat2("u_ScreenSize", static_cast<float>(InVpWidth), static_cast<float>(InVpHeight));
            } else {
                mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            }

            // Apply Material Pipeline State (Culling, Depth, Blend)
            const auto& pso = matInst->GetPipelineState();
            ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : pso.CullMode;
            FRenderCommand::SetCulling(cullMode != ECullMode::None, cullMode);
            FRenderCommand::SetDepthTesting(pso.bDepthTest);
            bool bDepthWrite = (matInst->GetAlphaMode() == EAlphaMode::Blend) ? false : pso.bDepthWrite;
            FRenderCommand::SetDepthMask(bDepthWrite);
            FRenderCommand::SetDepthFunc(pso.DepthFunc);
            bool bBlend = (matInst->GetAlphaMode() == EAlphaMode::Blend) || pso.bBlend;
            FRenderCommand::SetBlendState(bBlend);
            if (bBlend) {
                FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
            }

            // Bind resolved material parameters and textures (slots 0..5, 9)
            matInst->Bind(mesh.Shader);

            TRef<FTexture2D> lightmapTex;
            bool bUseLM = mesh.Mobility == EComponentMobility::Static && mesh.LightmapIndex >= 0 &&
                          !mesh.LightmapAssetPath.empty();
            if (bUseLM) {
                auto lm = UAssetManager::GetLightmap(mesh.LightmapAssetPath);
                if (lm)
                    lightmapTex = lm->GetOrCreateGPUTexture();
            }
            BindLightmapUniforms(*mesh.Shader, bUseLM, true, mesh.LightmapScale, mesh.LightmapBias, lightmapTex);

            // IBL enablement (samplers are statically bound to slots 6-8)
            mesh.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            mesh.Shader->SetInt("u_DebugMode", DebugMode);

            glm::mat4 model = transform.GetTransform();
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));

            // Audit fix MEDIO-05: compute normal matrix on CPU, not in vertex shader per-vertex
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // Static Mesh Component Rendering
        auto staticMeshView = reg.view<FTransformComponent, UStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, UStaticMeshComponent>(entity);
            if (!staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray())
                continue;
            if (IsStaticMeshCulled(transform, staticMeshComp, camFrustum))
                continue;

            TRef<FShader> activeShader = staticMeshComp.Shader
                                             ? staticMeshComp.Shader
                                             : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!activeShader)
                continue;

            activeShader->Bind();
            activeShader->SetInt("u_UseShadows", (bShadowsAvailable && staticMeshComp.bReceiveShadows) ? 1 : 0);
            activeShader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && staticMeshComp.bReceiveShadows) ? 1 : 0);
            activeShader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            activeShader->SetInt("u_DebugMode", DebugMode);

            staticMeshComp.StaticMesh->GetVertexArray()->Bind();

            const auto& submeshes = staticMeshComp.StaticMesh->GetSubmeshes();
            for (const auto& submesh : submeshes) {
                if (submesh.IndexCount == 0)
                    continue;

                // Resolve Material for this submesh
                TRef<FMaterialInstance> matInst =
                    ResolveStaticSubmeshMaterial(*staticMeshComp.StaticMesh, submesh, staticMeshComp.MaterialOverrides);

                // Planar reflection
                bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
                if (bApplyPlanarReflection) {
                    PlanarReflectionFramebuffer->BindTexture(0, 5);
                    activeShader->SetInt("u_UsePlanarReflection", 1);
                    activeShader->SetFloat2("u_ScreenSize", static_cast<float>(InVpWidth),
                                            static_cast<float>(InVpHeight));
                } else {
                    activeShader->SetInt("u_UsePlanarReflection", 0);
                }

                // Pipeline State
                const auto& pso = matInst->GetPipelineState();
                ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : pso.CullMode;
                FRenderCommand::SetCulling(cullMode != ECullMode::None, cullMode);
                FRenderCommand::SetDepthTesting(pso.bDepthTest);
                bool bDepthWrite = (matInst->GetAlphaMode() == EAlphaMode::Blend) ? false : pso.bDepthWrite;
                FRenderCommand::SetDepthMask(bDepthWrite);
                FRenderCommand::SetDepthFunc(pso.DepthFunc);
                bool bBlend = (matInst->GetAlphaMode() == EAlphaMode::Blend) || pso.bBlend;
                FRenderCommand::SetBlendState(bBlend);
                if (bBlend) {
                    FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
                }

                // Bind Material
                matInst->Bind(activeShader);

                TRef<FTexture2D> lightmapTex;
                bool bUseLM = staticMeshComp.Mobility == EComponentMobility::Static &&
                              staticMeshComp.LightmapIndex >= 0 && !staticMeshComp.LightmapAssetPath.empty();
                if (bUseLM) {
                    auto lm = UAssetManager::GetLightmap(staticMeshComp.LightmapAssetPath);
                    if (lm)
                        lightmapTex = lm->GetOrCreateGPUTexture();
                }
                BindLightmapUniforms(*activeShader, bUseLM, false, staticMeshComp.LightmapScale,
                                      staticMeshComp.LightmapBias, lightmapTex);

                // Transform with submesh local transform
                glm::mat4 model = transform.GetTransform() * submesh.LocalTransform;
                activeShader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
                activeShader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
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
    void FWorldRenderer::RenderSkyboxPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                          bool bHasDirLight, const FDirectionalLight& InDirLight) {
        if (!InSkybox || !InSkybox->bEnabled || !SkyboxShader || !SkyboxVA)
            return;

        FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
        FRenderCommand::SetDepthMask(false);

        SkyboxShader->Bind();
        SkyboxShader->SetMat4("u_View", glm::value_ptr(InCamera.GetViewMatrix()));
        SkyboxShader->SetMat4("u_Projection", glm::value_ptr(InCamera.GetProjectionMatrix()));

        if (InSkybox->bUseHDREnvironmentMap && InSkybox->HDREnvironmentMap) {
            InSkybox->HDREnvironmentMap->Bind(0);
            SkyboxShader->SetInt("u_UseHDREnvironmentMap", 1);
            SkyboxShader->SetInt("u_HDREnvironmentMap", 0);
        } else {
            SkyboxShader->SetInt("u_UseHDREnvironmentMap", 0);
            glm::vec3 sunDir = bHasDirLight ? -glm::normalize(InDirLight.Direction) : glm::vec3(0.f, 1.f, 0.f);
            SkyboxShader->SetFloat3("u_SunDir", sunDir.x, sunDir.y, sunDir.z);
            SkyboxShader->SetFloat3("u_SkyColor", InSkybox->SkyZenithColor.r, InSkybox->SkyZenithColor.g,
                                      InSkybox->SkyZenithColor.b);
            SkyboxShader->SetFloat3("u_HorizonColor", InSkybox->HorizonColor.r, InSkybox->HorizonColor.g,
                                      InSkybox->HorizonColor.b);
            SkyboxShader->SetFloat3("u_GroundColor", InSkybox->GroundColor.r, InSkybox->GroundColor.g,
                                      InSkybox->GroundColor.b);
            SkyboxShader->SetFloat3("u_SunColor", InSkybox->SunColor.r, InSkybox->SunColor.g, InSkybox->SunColor.b);
            SkyboxShader->SetFloat("u_SunIntensity", InSkybox->SunIntensity);
        }

        SkyboxVA->Bind();
        FRenderCommand::DrawIndexed(SkyboxVA);

        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
    }

    // =========================================================================
    // Post-Process Pass (Bloom + ACES Tonemapping + FXAA)
    // =========================================================================
    void FWorldRenderer::RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth,
                                               uint32_t InVpHeight) {
        if (!HDRSceneFramebuffer)
            return;

        PostProcessSettings.Exposure = InExposure;
        PostProcessPipeline.Render(PostProcessSettings, HDRSceneFramebuffer, InTargetFBO, InVpWidth, InVpHeight);
    }

    // =========================================================================
    // IBL update (triggered when HDR path changes)
    // =========================================================================
    void FWorldRenderer::UpdateIBL(const FSkyboxComponent& InSkybox) {
        std::string currentHdr = InSkybox.HDREnvironmentMapPath;
        if (currentHdr.empty() && InSkybox.HDREnvironmentMap)
            currentHdr = InSkybox.HDREnvironmentMap->GetPath();

        if (!bEnvironmentGenerated ||
            (InSkybox.bUseHDREnvironmentMap && !currentHdr.empty() && currentHdr != LoadedHDRPath) ||
            (!InSkybox.bUseHDREnvironmentMap && !LoadedHDRPath.empty())) {
            IBLEnvironment = FIBLGenerator::CreateEnvironmentFromSkybox(InSkybox);
            LoadedHDRPath = InSkybox.bUseHDREnvironmentMap ? currentHdr : "";
            bEnvironmentGenerated = true;
        }
    }

} // namespace Leon
