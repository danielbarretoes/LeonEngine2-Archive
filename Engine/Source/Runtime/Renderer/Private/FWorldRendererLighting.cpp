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

    // =========================================================================
    // PASS 1: Cascaded Shadow Pass (OpenGL 4.5 Texture2DArray)
    // =========================================================================
    void FWorldRenderer::RenderCascadedShadowPass(const FPerspectiveCamera& InCamera,
                                                  const FDirectionalLightComponent* InDirLightComp,
                                                  FCameraBufferData& OutCamData) {
        if (!InDirLightComp || !InDirLightComp->bEnabled || !ShadowDepthShader || !CascadeShadowFramebuffer)
            return;
        if (ShadowSettings.CascadeCount == 0)
            return;

        float nearClip = InCamera.GetNearClip();
        float farClip = ShadowSettings.ShadowDistance;

        auto splits = ShadowMath::CalculateCascadeSplits(ShadowSettings.CascadeCount, nearClip, farClip,
                                                         ShadowSettings.SplitLambda, ShadowSettings.SplitScheme);
        // UBO always stores 4 far planes. Unused cascades repeat the last split.
        float splitPlane[5] = {nearClip, farClip, farClip, farClip, farClip};
        for (size_t i = 0; i < splits.size() && i < 5; ++i)
            splitPlane[i] = splits[i];

        OutCamData.CascadeSplits = glm::vec4(splitPlane[1], splitPlane[2], splitPlane[3], splitPlane[4]);
        OutCamData.ShadowParams = glm::vec4(ShadowSettings.ConstantBias, ShadowSettings.SlopeBias,
                                            ShadowSettings.NormalBias, ShadowSettings.CascadeBlendWidth);
        OutCamData.ShadowSettings =
            glm::ivec4(static_cast<int>(ShadowSettings.FilterMode), ShadowSettings.ShadowedSpotIndex, 0, DebugMode);

        CascadeShadowFramebuffer->Bind();
        FRenderCommand::SetViewport(0, 0, ShadowSettings.CascadeResolution, ShadowSettings.CascadeResolution);
        ResetDefaultMeshRasterState();
        FRenderCommand::SetCulling(true, ECullMode::Front);
        FRenderCommand::SetPolygonOffset(true, 2.0f, 4.0f);

        ShadowDepthShader->Bind();

        float fov = InCamera.GetFOV();
        float aspect = InCamera.GetAspectRatio();

        for (uint32_t cascade = 0; cascade < ShadowSettings.CascadeCount && cascade < 4; ++cascade) {
            CascadeShadowFramebuffer->AttachDepthTextureLayer(cascade);
            FRenderCommand::Clear();

            glm::mat4 subProj;
            {
                float sliceNear = splitPlane[cascade];
                float sliceFar = splitPlane[cascade + 1];
                ShadowMath::CascadeSliceDepthRange(cascade, splits, ShadowSettings.CascadeBlendWidth,
                                                   FShadowSettings::kCascadeBlendMinMeters, sliceNear, sliceFar);
                subProj = glm::perspective(glm::radians(fov), aspect, sliceNear, sliceFar);
            }
            auto corners = ShadowMath::GetFrustumCornersWorldSpace(subProj, InCamera.GetViewMatrix());

            float worldUnitsPerTexel = 0.01f;
            glm::mat4 cascadeMatrix = ShadowMath::CalculateCascadeMatrix(
                corners, InDirLightComp->Light.Direction, ShadowSettings.CascadeResolution,
                ShadowSettings.bStabilizeCascades, worldUnitsPerTexel);
            OutCamData.LightSpaceMatrices[cascade] = cascadeMatrix;

            ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));

            auto meshView = World->GetRegistry().view<FTransformComponent, FMeshComponent>();
            for (auto entity : meshView) {
                auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
                if (!mesh.VertexArray || !mesh.bCastShadows || !mesh.bVisible)
                    continue;

                glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform);
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
            auto staticMeshView = World->GetRegistry().view<FTransformComponent, FStaticMeshComponent>();
            for (auto entity : staticMeshView) {
                auto [transform, staticMeshComp] =
                    staticMeshView.get<FTransformComponent, FStaticMeshComponent>(entity);
                if (!staticMeshComp.bVisible || !staticMeshComp.StaticMesh ||
                    !staticMeshComp.StaticMesh->GetVertexArray() || !staticMeshComp.bCastShadows)
                    continue;

                staticMeshComp.StaticMesh->GetVertexArray()->Bind();
                for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                    if (submesh.IndexCount == 0)
                        continue;
                    glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform) * submesh.LocalTransform;
                    ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                    ShadowDepthShader->SetInt("u_AlphaMode", 0);
                    ShadowDepthShader->SetInt("u_UseAlbedoMap", 0);
                    FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                      submesh.IndexOffset);
                }
            }

            if (ShadowDepthSkinnedShader) {
                ShadowDepthSkinnedShader->Bind();
                ShadowDepthSkinnedShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));
                auto skelView = World->GetRegistry().view<FTransformComponent, FSkinnedMeshRenderState>();
                for (auto entity : skelView) {
                    auto [transform, skel] = skelView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
                    if (!skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray() || !skel.bCastShadows ||
                        !skel.bVisible)
                        continue;
                    UploadBonePalette(BonePaletteUBO.get(), skel.BonePalette);
                    skel.SkeletalMesh->GetVertexArray()->Bind();
                    for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                        if (submesh.IndexCount == 0)
                            continue;
                        glm::mat4 model = SkeletalModelMatrix(ResolveActorWorldMatrix(World, entity, transform), skel,
                                                              submesh.LocalTransform);
                        ShadowDepthSkinnedShader->SetMat4("u_Model", glm::value_ptr(model));
                        ShadowDepthSkinnedShader->SetInt("u_AlphaMode", 0);
                        ShadowDepthSkinnedShader->SetInt("u_UseAlbedoMap", 0);
                        FRenderCommand::DrawIndexedOffset(skel.SkeletalMesh->GetVertexArray(), submesh.IndexCount,
                                                          submesh.IndexOffset);
                    }
                }
                ShadowDepthShader->Bind();
                ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(cascadeMatrix));
            }
        }

        FRenderCommand::SetPolygonOffset(false);
        ResetDefaultMeshRasterState();
        CascadeShadowFramebuffer->Unbind();
    }

    // =========================================================================
    // PASS 2: Spot Shadow Pass
    // =========================================================================
    void FWorldRenderer::RenderSpotShadowPass(const FSpotLightComponent* InSpotLightComp,
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
        FRenderCommand::SetViewport(0, 0, ShadowSettings.SpotResolution, ShadowSettings.SpotResolution);
        FRenderCommand::Clear();
        ResetDefaultMeshRasterState();
        FRenderCommand::SetPolygonOffset(true, 2.0f, 4.0f);

        ShadowDepthShader->Bind();
        ShadowDepthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(spotLightSpace));

        auto meshView = World->GetRegistry().view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.bCastShadows || !mesh.bVisible)
                continue;
            glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform);
            ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        auto staticMeshView = World->GetRegistry().view<FTransformComponent, FStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, FStaticMeshComponent>(entity);
            if (!staticMeshComp.bVisible || !staticMeshComp.StaticMesh ||
                !staticMeshComp.StaticMesh->GetVertexArray() || !staticMeshComp.bCastShadows)
                continue;
            staticMeshComp.StaticMesh->GetVertexArray()->Bind();
            for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;
                glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform) * submesh.LocalTransform;
                ShadowDepthShader->SetMat4("u_Model", glm::value_ptr(model));
                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        if (ShadowDepthSkinnedShader) {
            ShadowDepthSkinnedShader->Bind();
            ShadowDepthSkinnedShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(spotLightSpace));
            auto skelView = World->GetRegistry().view<FTransformComponent, FSkinnedMeshRenderState>();
            for (auto entity : skelView) {
                auto [transform, skel] = skelView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
                if (!skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray() || !skel.bCastShadows || !skel.bVisible)
                    continue;
                UploadBonePalette(BonePaletteUBO.get(), skel.BonePalette);
                skel.SkeletalMesh->GetVertexArray()->Bind();
                for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                    if (submesh.IndexCount == 0)
                        continue;
                    glm::mat4 model = SkeletalModelMatrix(ResolveActorWorldMatrix(World, entity, transform), skel,
                                                          submesh.LocalTransform);
                    ShadowDepthSkinnedShader->SetMat4("u_Model", glm::value_ptr(model));
                    FRenderCommand::DrawIndexedOffset(skel.SkeletalMesh->GetVertexArray(), submesh.IndexCount,
                                                      submesh.IndexOffset);
                }
            }
        }

        FRenderCommand::SetPolygonOffset(false);
        ResetDefaultMeshRasterState();
        SpotShadowFramebuffer->Unbind();
    }

    void FWorldRenderer::ClearPlanarReflectionPlanes() {
        PlanarReflectionPlanes.clear();
    }

    void FWorldRenderer::AddPlanarReflectionPlane(const glm::vec3& InNormal, float InDistance) {
        glm::vec3 n = SafeNormalize(InNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        PlanarReflectionPlanes.push_back({n, InDistance});
    }

    FWorldRenderer::FPlanarReflectionPlane FWorldRenderer::SelectFloorPlane() const {
        for (const auto& plane : PlanarReflectionPlanes) {
            if (IsHorizontalPlanarPlane(plane.Normal))
                return {SafeNormalize(plane.Normal, glm::vec3(0.0f, 1.0f, 0.0f)), plane.Distance};
        }
        return {{0.0f, 1.0f, 0.0f}, 0.0f};
    }

    bool FWorldRenderer::SelectWallMirrorPlane(const FPerspectiveCamera& InCamera,
                                               FPlanarReflectionPlane& OutPlane) const {
        const glm::vec3 camPos = InCamera.GetPosition();
        const glm::vec3 camFwd = InCamera.GetForwardDirection();
        float bestScore = kWallPlanarCaptureMinScore;
        bool found = false;
        for (const auto& plane : PlanarReflectionPlanes) {
            if (IsHorizontalPlanarPlane(plane.Normal))
                continue;
            const glm::vec3 n = SafeNormalize(plane.Normal, glm::vec3(0.0f, 0.0f, 1.0f));
            const float score = PlanarReflectionPlaneScore(n, plane.Distance, camPos, camFwd);
            if (score > bestScore) {
                bestScore = score;
                OutPlane = {n, plane.Distance};
                found = true;
            }
        }
        return found;
    }

    // =========================================================================
    // PASS 3: Planar Reflection Pass
    // Floor is always captured. A facing wall mirror uses a second FBO so the wet floor
    // stays live while looking at a mirror (Unreal PlanarReflection actors, not probes).
    // =========================================================================
    void FWorldRenderer::RenderPlanarReflectionPass(const FPerspectiveCamera& InCamera,
                                                    const FSkyboxComponent* InSkybox, bool bHasDirLight,
                                                    const FDirectionalLight& InDirLight) {
        bWallPlanarActive = false;
        if (!bEnablePlanarReflection || !PlanarReflectionFramebuffer)
            return;

        const FPlanarReflectionPlane floor = SelectFloorPlane();
        PlanarPlaneNormal = floor.Normal;
        PlanarPlaneDistance = floor.Distance;
        CapturePlanarReflection(InCamera, InSkybox, bHasDirLight, InDirLight, floor, *PlanarReflectionFramebuffer,
                                PlanarViewProjection);

        FPlanarReflectionPlane wall;
        if (WallPlanarReflectionFramebuffer && SelectWallMirrorPlane(InCamera, wall)) {
            WallPlanarPlaneNormal = wall.Normal;
            WallPlanarPlaneDistance = wall.Distance;
            CapturePlanarReflection(InCamera, InSkybox, bHasDirLight, InDirLight, wall,
                                    *WallPlanarReflectionFramebuffer, WallPlanarViewProjection);
            bWallPlanarActive = true;
        }
    }

    void FWorldRenderer::CapturePlanarReflection(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                                 bool bHasDirLight, const FDirectionalLight& InDirLight,
                                                 const FPlanarReflectionPlane& InPlane, FFramebuffer& InTarget,
                                                 glm::mat4& OutViewProjection) {
        uint32_t vpW = InTarget.GetSpecification().Width;
        uint32_t vpH = InTarget.GetSpecification().Height;
        if (vpW == 0)
            vpW = PlanarCaptureWidth();
        if (vpH == 0)
            vpH = PlanarCaptureHeight();

        glm::vec3 camPos = InCamera.GetPosition();
        glm::mat4 reflectMatrix = PlanarReflectionMatrix(InPlane.Normal, InPlane.Distance);
        glm::mat4 mirrorView = InCamera.GetViewMatrix() * reflectMatrix;
        glm::mat4 mirrorProj = InCamera.GetProjectionMatrix();
        glm::mat4 mirroredVP = mirrorProj * mirrorView;
        OutViewProjection = mirroredVP;

        glm::vec3 mirrorPos = ReflectPointThroughPlane(camPos, InPlane.Normal, InPlane.Distance);
        glm::vec3 mirrorFwd = glm::mat3(reflectMatrix) * InCamera.GetForwardDirection();
        if (glm::dot(mirrorFwd, mirrorFwd) < 1e-8f)
            mirrorFwd = glm::vec3(0.0f, 0.0f, -1.0f);
        else
            mirrorFwd = glm::normalize(mirrorFwd);

        if (CameraUBO) {
            FCameraBufferData mirrorCamData;
            mirrorCamData.ViewProjection = mirroredVP;
            mirrorCamData.CameraPosition = glm::vec4(mirrorPos, 1.0f);
            mirrorCamData.CameraForward = glm::vec4(mirrorFwd, 0.0f);
            CameraUBO->SetData(&mirrorCamData, sizeof(FCameraBufferData), 0);
        }

        InTarget.Bind();
        FRenderCommand::SetViewport(0, 0, vpW, vpH);
        FRenderCommand::SetClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        FRenderCommand::Clear();
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        // Skybox.glsl does not write gl_ClipDistance; enabling clip first culls the entire cube.
        FRenderCommand::SetClipDistance(false);

        if (InSkybox && InSkybox->bEnabled && SkyboxShader && SkyboxVA) {
            FRenderCommand::SetCulling(false);
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetDepthMask(false);
            SkyboxShader->Bind();
            SkyboxShader->SetMat4("u_View", glm::value_ptr(mirrorView));
            SkyboxShader->SetMat4("u_Projection", glm::value_ptr(mirrorProj));
            SkyboxShader->SetFloat("u_EnvironmentIntensity", InSkybox->EnvironmentIntensity);
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

        FRenderCommand::SetClipDistance(true);

        if (CascadeShadowFramebuffer)
            CascadeShadowFramebuffer->BindDepthTexture(10);
        if (SpotShadowFramebuffer)
            SpotShadowFramebuffer->BindDepthTexture(11);

        if (DefaultWhiteTexture) {
            DefaultWhiteTexture->Bind(0);
            DefaultWhiteTexture->Bind(2);
            DefaultWhiteTexture->Bind(3);
            DefaultWhiteTexture->Bind(4);
        }
        if (DefaultFlatNormalTexture)
            DefaultFlatNormalTexture->Bind(1);
        if (DefaultBlackTexture) {
            DefaultBlackTexture->Bind(5);
            DefaultBlackTexture->Bind(13);
        }

        bool bIBLAvailable = bUseIBL && IBLEnvironment.BRDFLUT;
        if (bIBLAvailable) {
            if (IBLEnvironment.BRDFLUT)
                IBLEnvironment.BRDFLUT->Bind(6);
            if (IBLEnvironment.IrradianceMap)
                IBLEnvironment.IrradianceMap->Bind(7);
            if (IBLEnvironment.PrefilterMap)
                IBLEnvironment.PrefilterMap->Bind(8);
        }

        auto CullModeForReflection = [](ECullMode InMode) -> ECullMode {
            if (InMode == ECullMode::Back)
                return ECullMode::Front;
            if (InMode == ECullMode::Front)
                return ECullMode::Back;
            return InMode;
        };
        auto NormalMatrixForReflection = [&reflectMatrix](const glm::mat4& InModel) -> glm::mat3 {
            return SafeNormalMatrix(reflectMatrix * InModel);
        };

        auto& reg = World->GetRegistry();
        const FFrustumPlanes reflectionFrustum = ExtractFrustumPlanes(mirroredVP);
        auto meshView = reg.view<FTransformComponent, FMeshComponent>();
        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader ||
                !CanContributeToPlanarReflection(mesh.bVisible, mesh.bVisibleInReflection, mesh.Mobility))
                continue;
            glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
            if (IsProceduralMeshCulled(world, mesh, reflectionFrustum))
                continue;

            mesh.Shader->Bind();
            mesh.Shader->SetInt("u_UsePlanarReflection", 0);
            mesh.Shader->SetInt("u_UsePlanarReflection1", 0);
            mesh.Shader->SetInt("u_UseShadows", 0);
            mesh.Shader->SetInt("u_UseSpotShadows", 0);
            mesh.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            mesh.Shader->SetInt("u_DebugMode", 0);
            mesh.Shader->SetInt("u_UseInstancing", 0);
            mesh.Shader->SetInt("u_EnableClipPlane", 1);
            mesh.Shader->SetFloat4("u_ClipPlane", InPlane.Normal.x, InPlane.Normal.y, InPlane.Normal.z,
                                   InPlane.Distance);

            TRef<FMaterialInstance> matInst = nullptr;
            if (reg.all_of<FMaterialComponent>(entity)) {
                matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
            }
            if (!matInst) {
                matInst = UAssetManager::GetDefaultMaterialInstance();
            }

            glm::mat4 model = world;
            const auto& pso = matInst->GetPipelineState();
            ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : CullModeForReflection(pso.CullMode);
            cullMode = FlipCullForNegativeScale(cullMode, model);
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
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            glm::mat3 normalMatrix = NormalMatrixForReflection(model);
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        auto staticMeshView = reg.view<FTransformComponent, FStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, FStaticMeshComponent>(entity);
            if (!staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray() ||
                !CanContributeToPlanarReflection(staticMeshComp.bVisible, staticMeshComp.bVisibleInReflection,
                                                 staticMeshComp.Mobility))
                continue;
            glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
            if (IsStaticMeshCulled(world, staticMeshComp, reflectionFrustum))
                continue;

            TRef<FShader> shader = staticMeshComp.Shader
                                       ? staticMeshComp.Shader
                                       : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!shader)
                continue;

            shader->Bind();
            shader->SetInt("u_UsePlanarReflection", 0);
            shader->SetInt("u_UsePlanarReflection1", 0);
            shader->SetInt("u_UseShadows", 0);
            shader->SetInt("u_UseSpotShadows", 0);
            shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            shader->SetInt("u_DebugMode", 0);
            shader->SetInt("u_UseInstancing", 0);
            shader->SetInt("u_EnableClipPlane", 1);
            shader->SetFloat4("u_ClipPlane", InPlane.Normal.x, InPlane.Normal.y, InPlane.Normal.z, InPlane.Distance);

            staticMeshComp.StaticMesh->GetVertexArray()->Bind();
            for (const auto& submesh : staticMeshComp.StaticMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;

                TRef<FMaterialInstance> matInst =
                    ResolveStaticSubmeshMaterial(*staticMeshComp.StaticMesh, submesh, staticMeshComp.MaterialOverrides);

                glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform) * submesh.LocalTransform;
                const auto& pso = matInst->GetPipelineState();
                ECullMode cullMode = matInst->GetDoubleSided() ? ECullMode::None : CullModeForReflection(pso.CullMode);
                cullMode = FlipCullForNegativeScale(cullMode, model);
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

                shader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = NormalMatrixForReflection(model);
                shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        auto skelView = reg.view<FTransformComponent, FSkinnedMeshRenderState>();
        for (auto entity : skelView) {
            auto [transform, skel] = skelView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
            if (!skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray() || !skel.bVisible ||
                !skel.bVisibleInReflection)
                continue;
            glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
            if (IsSkeletalMeshCulled(world, skel, reflectionFrustum))
                continue;
            TRef<FShader> shader =
                skel.Shader ? skel.Shader : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Skinned.glsl");
            if (!shader)
                continue;
            UploadBonePalette(BonePaletteUBO.get(), skel.BonePalette);
            shader->Bind();
            shader->SetInt("u_UsePlanarReflection", 0);
            shader->SetInt("u_UsePlanarReflection1", 0);
            shader->SetInt("u_UseShadows", 0);
            shader->SetInt("u_UseSpotShadows", 0);
            shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            shader->SetInt("u_DebugMode", 0);
            shader->SetInt("u_UseInstancing", 0);
            shader->SetInt("u_EnableClipPlane", 1);
            shader->SetFloat4("u_ClipPlane", InPlane.Normal.x, InPlane.Normal.y, InPlane.Normal.z, InPlane.Distance);
            skel.SkeletalMesh->GetVertexArray()->Bind();
            for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;
                TRef<FMaterialInstance> matInst =
                    ResolveSkeletalSubmeshMaterial(*skel.SkeletalMesh, submesh, skel.MaterialOverrides);
                glm::mat4 model = SkeletalModelMatrix(ResolveActorWorldMatrix(World, entity, transform), skel,
                                                      submesh.LocalTransform);
                ApplyMeshRasterState(*matInst, model, matInst->GetAlphaMode() == EAlphaMode::Blend);
                matInst->Bind(shader);
                shader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = NormalMatrixForReflection(model);
                shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                FRenderCommand::DrawIndexedOffset(skel.SkeletalMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);

        auto textView = reg.view<FTransformComponent, FTextComponent>();
        FTextRenderer::BeginScene(InCamera);
        for (auto entity : textView) {
            auto [transform, textComp] = textView.get<FTransformComponent, FTextComponent>(entity);
            if (textComp.Text.empty())
                continue;
            FTextRenderer::DrawString(textComp.Text, transform.GetTransform(), textComp.Color, textComp.Size,
                                      textComp.Alignment, textComp.bDoubleSided);
        }
        FTextRenderer::EndScene();

        InTarget.GenerateColorMipmaps();
        InTarget.Unbind();
        FRenderCommand::SetClipDistance(false);
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
