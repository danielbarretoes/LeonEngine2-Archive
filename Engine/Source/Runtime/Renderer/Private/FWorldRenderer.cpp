#include "Renderer/FWorldRenderer.hpp"
#include "FWorldRendererInternals.hpp"
#include "Core/FLog.hpp"
#include "Assets/UAssetManager.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/FLightmapAsset.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FFramebuffer.hpp"
#include "Renderer/FFrustumCull.hpp"
#include "Renderer/FIBLGenerator.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FRenderer.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Renderer/FTextRenderer.hpp"
#include "Renderer/FParticleRenderer.hpp"
#include "RHI/FVertexArray.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/FGameplayDebugger.hpp"
#include "Physics/FCollisionQuery.hpp"
#include "AI/UNavigationSystem.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"

#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <limits>
#include <vector>

namespace Leon {

    FWorldRenderer::FWorldRenderer(UWorld* InWorld) : World(InWorld) {
        if (InWorld && InWorld->HasPendingRendererDefaults()) {
            ShadowSettings.CascadeResolution = InWorld->GetPendingShadowMapResolution();
            ShadowSettings.CascadeCount = InWorld->GetPendingCascadeCount();
            ShadowSettings.ShadowDistance = InWorld->GetPendingShadowDistance();
            bEnablePlanarReflection = InWorld->GetPendingPlanarReflectionEnabled();
            PlanarQuality = InWorld->GetPendingPlanarReflectionQuality();
            PlanarResolutionScale = InWorld->GetPendingPlanarReflectionResolutionScale();
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
        planarSpec.Width = PlanarCaptureWidth();
        planarSpec.Height = PlanarCaptureHeight();
        // Must be HDR: PBR_Lit + Skybox write linear radiance > 1. RGBA8 clamps to white blobs on mirrors/wet floors.
        // ColorMipLevels follow quality (Epic = 5 → roughness * 4.0 LOD, matches prefilter).
        planarSpec.ColorMipLevels = PlanarReflectionMipLevelsFor(PlanarQuality);
        planarSpec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
        PlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);
        WallPlanarReflectionFramebuffer = FFramebuffer::Create(planarSpec);

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
        BonePaletteUBO = FUniformBuffer::Create(static_cast<unsigned int>(sizeof(glm::mat4) * kMaxBones), 2);

        // -----------------------------------------------------------------------
        // 4. Pipeline shaders and geometry
        // -----------------------------------------------------------------------
        ShadowDepthShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth.glsl");
        ShadowDepthSkinnedShader = FShader::Create("Engine/Assets/Shaders/ShadowDepth_Skinned.glsl");
        SkyboxShader = FShader::Create("Engine/Assets/Shaders/Skybox.glsl");

        SkyboxVA = FMeshPrimitives::CreateCube(2.0f);

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

    void FWorldRenderer::ApplyProjectRendererDefaults(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection,
                                                      EPlanarReflectionQuality InPlanarQuality,
                                                      float InPlanarResolutionScale) {
        bEnablePlanarReflection = bInEnablePlanarReflection;
        PlanarQuality = InPlanarQuality;
        PlanarResolutionScale = InPlanarResolutionScale > 0.0f
                                    ? ClampPlanarReflectionResolutionScale(InPlanarResolutionScale)
                                    : PlanarReflectionScaleFor(InPlanarQuality);
        EnsurePlanarFramebuffers();

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

    void FWorldRenderer::SetPlanarReflectionQuality(EPlanarReflectionQuality InQuality) {
        PlanarQuality = InQuality;
        PlanarResolutionScale = PlanarReflectionScaleFor(InQuality);
        EnsurePlanarFramebuffers();
    }

    void FWorldRenderer::SetPlanarReflectionResolutionScale(float InScale) {
        PlanarResolutionScale = ClampPlanarReflectionResolutionScale(InScale);
        EnsurePlanarFramebuffers();
    }

    uint32_t FWorldRenderer::PlanarCaptureWidth() const {
        uint32_t w = ViewportWidth > 0 ? ViewportWidth : 1280;
        return std::max(8u, static_cast<uint32_t>(static_cast<float>(w) * PlanarResolutionScale + 0.5f));
    }

    uint32_t FWorldRenderer::PlanarCaptureHeight() const {
        uint32_t h = ViewportHeight > 0 ? ViewportHeight : 720;
        return std::max(8u, static_cast<uint32_t>(static_cast<float>(h) * PlanarResolutionScale + 0.5f));
    }

    void FWorldRenderer::EnsurePlanarFramebuffers() {
        const uint32_t w = PlanarCaptureWidth();
        const uint32_t h = PlanarCaptureHeight();
        const uint32_t mips = PlanarReflectionMipLevelsFor(PlanarQuality);
        auto recreateIfNeeded = [&](TRef<FFramebuffer>& InTarget) {
            if (!InTarget || InTarget->GetSpecification().ColorMipLevels != mips) {
                FFramebufferSpecification spec;
                spec.Width = w;
                spec.Height = h;
                spec.ColorMipLevels = mips;
                spec.Attachments = {EFramebufferTextureFormat::RGBA16F, EFramebufferTextureFormat::DEPTH24STENCIL8};
                InTarget = FFramebuffer::Create(spec);
                return;
            }
            if (InTarget->GetSpecification().Width != w || InTarget->GetSpecification().Height != h)
                InTarget->Resize(w, h);
        };
        recreateIfNeeded(PlanarReflectionFramebuffer);
        recreateIfNeeded(WallPlanarReflectionFramebuffer);
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
            EnsurePlanarFramebuffers();
            PostProcessPipeline.OnViewportResize(InWidth, InHeight);
        }
    }

    // =========================================================================
    // Main Render — frame entry point
    // =========================================================================
    void FWorldRenderer::Render(const FPerspectiveCamera& InCamera) {
        if (!World)
            return;

        FRenderer::ResetStats();
        FFrameProfiler::Working().ShadowDrawCalls = 0;

        auto& reg = World->GetRegistry();

        // Audit fix FASE-8: track FBO in CPU instead of querying with glGetIntegerv per frame
        PreviousFBO = FRenderCommand::GetFramebufferBinding();

        uint32_t vpWidth = ViewportWidth > 0 ? ViewportWidth : 1280;
        uint32_t vpHeight = ViewportHeight > 0 ? ViewportHeight : 720;

        // ------------------------------------------------------------------
        // Gather lights
        // ------------------------------------------------------------------
        bool bHasDirLight = false;
        FDirectionalLightComponent dirLightComp;
        {
            auto view = reg.view<FDirectionalLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<FDirectionalLightComponent>(entity);
                if (comp.bEnabled && comp.Mobility != ELightMobility::Static) {
                    dirLightComp = comp;
                    bHasDirLight = true;
                    break;
                }
            }
        }

        std::vector<FPointLight> pointLights;
        {
            auto view = reg.view<FPointLightComponent>();
            for (auto entity : view) {
                const auto& comp = view.get<FPointLightComponent>(entity);
                if (comp.bEnabled && comp.Mobility != ELightMobility::Static && pointLights.size() < 16) {
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
            FFrameProfiler::FScope ibl(&FFrameProfiler::Working().IBLMs);
            UpdateIBL(skybox);
        } else if (!bEnvironmentGenerated) {
            FFrameProfiler::FScope ibl(&FFrameProfiler::Working().IBLMs);
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
        const uint32_t drawsBeforeShadow = FRenderer::GetStats().DrawCalls;
        if (bHasDirLight) {
            FFrameProfiler::FScope shadow(&FFrameProfiler::Working().ShadowMs);
            RenderCascadedShadowPass(InCamera, &dirLightComp, mainCamData);
        }
        if (bHasSpotLight) {
            FFrameProfiler::FScope shadow(&FFrameProfiler::Working().ShadowMs);
            RenderSpotShadowPass(&firstSpotComp, firstSpotPos, mainCamData);
        }
        FFrameProfiler::Working().ShadowDrawCalls = FRenderer::GetStats().DrawCalls - drawsBeforeShadow;

        {
            FFrameProfiler::FScope planar(&FFrameProfiler::Working().TransparentMs);
            RenderPlanarReflectionPass(InCamera, bHasSkybox ? &skybox : nullptr, bHasDirLight, dirLightComp.Light);
        }

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

        if (CameraUBO)
            CameraUBO->SetData(&mainCamData, sizeof(FCameraBufferData), 0);

        // Wireframe applies only to 3D scene content — never to post-process / UI / overlays
        if (bWireframeEnabled) {
            FRenderCommand::SetWireframe(true);
        }

        {
            FFrameProfiler::FScope opaque(&FFrameProfiler::Working().OpaqueMs);
            RenderGeometryPass(InCamera, bHasDirLight, bHasSpotLight, vpWidth, vpHeight);
        }

        if (bHasSkybox) {
            FFrameProfiler::FScope sky(&FFrameProfiler::Working().SkyMs);
            RenderSkyboxPass(InCamera, &skybox, bHasDirLight, dirLightComp.Light);
        }

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

        {
            FFrameProfiler::FScope particles(&FFrameProfiler::Working().TransparentMs);
            FParticleRenderer::Render(World, InCamera);
        }

        FRenderCommand::SetWireframe(false);

        // Gameplay debug must run on the HDR target while scene depth is still valid
        // (post-process blits color only — occluding traces/colliders requires this pass).
        const bool bGameplayDebug = FGameplayDebugger::IsEnabled();
        if (bGameplayDebug) {
            FRenderCommand::SetDepthTesting(true);
            FRenderCommand::SetDepthMask(false);
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FDebugRenderer::BeginScene(InCamera);
            if (FGameplayDebugger::ShowPhysics())
                DrawDebugWorldColliders(*World);
            if (FGameplayDebugger::ShowAI() && World->GetNavigationSystem() && World->GetNavigationSystem()->IsBuilt())
                World->GetNavigationSystem()->DrawDebug();
            FDebugRenderer::DrawQueuedTraces();
            FDebugRenderer::EndScene(true);
            FDebugRenderer::ClearQueuedTraces();
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        }

        if (HDRSceneFramebuffer)
            HDRSceneFramebuffer->Unbind();

        {
            FFrameProfiler::FScope pp(&FFrameProfiler::Working().PostProcessMs);
            RenderPostProcessPass(skybox.Exposure, PreviousFBO, vpWidth, vpHeight);
        }

        const auto& stats = FRenderer::GetStats();
        FFrameProfiler::Working().VisibleActors = static_cast<int32_t>(stats.MeshesDrawn);
        FFrameProfiler::Working().CulledActors = static_cast<int32_t>(stats.MeshesCulled);
    }

} // namespace Leon
