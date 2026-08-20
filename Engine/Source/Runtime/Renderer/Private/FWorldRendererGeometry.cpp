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

    namespace {
        std::vector<FTransparentDraw> GPendingTransparents;
        std::vector<FOpaqueDraw> GPendingOpaques;

        void SubmitOpaqueDraw(FOpaqueDraw InDraw) { GPendingOpaques.push_back(std::move(InDraw)); }
    }

    void FWorldRenderer::BindPlanarReflectionUniforms(FShader& InShader, bool bEnabled) {
        if (bEnabled && PlanarReflectionFramebuffer) {
            PlanarReflectionFramebuffer->BindTexture(0, 5);
            InShader.SetInt("u_UsePlanarReflection", 1);
            InShader.SetMat4("u_PlanarViewProjection", glm::value_ptr(PlanarViewProjection));
            InShader.SetFloat3("u_PlanarPlaneNormal", PlanarPlaneNormal.x, PlanarPlaneNormal.y, PlanarPlaneNormal.z);
            InShader.SetFloat("u_PlanarPlaneDistance", PlanarPlaneDistance);
        } else {
            InShader.SetInt("u_UsePlanarReflection", 0);
        }
        if (bEnabled && bWallPlanarActive && WallPlanarReflectionFramebuffer) {
            WallPlanarReflectionFramebuffer->BindTexture(0, 13);
            InShader.SetInt("u_UsePlanarReflection1", 1);
            InShader.SetMat4("u_PlanarViewProjection1", glm::value_ptr(WallPlanarViewProjection));
            InShader.SetFloat3("u_PlanarPlaneNormal1", WallPlanarPlaneNormal.x, WallPlanarPlaneNormal.y,
                               WallPlanarPlaneNormal.z);
            InShader.SetFloat("u_PlanarPlaneDistance1", WallPlanarPlaneDistance);
        } else {
            if (DefaultBlackTexture)
                DefaultBlackTexture->Bind(13);
            InShader.SetInt("u_UsePlanarReflection1", 0);
        }
    }

    // =========================================================================
    // PASS 4: Opaque geometry (transparents are flushed after the skybox)
    // =========================================================================
    void FWorldRenderer::RenderOpaqueGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight,
                                                  bool bHasSpotLight) {
        auto& reg = World->GetRegistry();
        GPendingTransparents.clear();
        GPendingOpaques.clear();

        // --- Bind per-frame textures ONCE (shadow maps + IBL) ---
        // Bind shadow depth maps to slots 10-11
        if (CascadeShadowFramebuffer)
            CascadeShadowFramebuffer->BindDepthTexture(10);
        if (SpotShadowFramebuffer)
            SpotShadowFramebuffer->BindDepthTexture(11);
        if (PointShadowFramebuffer)
            PointShadowFramebuffer->BindDepthTexture(14);

        bool bShadowsOn = ShadowSettings.bEnableShadows;
        bool bShadowsAvailable = bShadowsOn && bHasDirLight && CascadeShadowFramebuffer;
        bool bSpotShadowAvailable = bShadowsOn && bHasSpotLight && SpotShadowFramebuffer;
        bool bPointShadowAvailable = bShadowsOn && PointShadowFramebuffer;

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
        glm::vec3 camPos = InCamera.GetPosition();

        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisible)
                continue;

            glm::mat4 model = ResolveActorWorldMatrix(World, entity, transform);
            if (IsProceduralMeshCulled(model, mesh, camFrustum))
                continue;

            TRef<FMaterialInstance> matInst = nullptr;
            if (reg.all_of<FMaterialComponent>(entity)) {
                matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
            }
            if (!matInst) {
                matInst = UAssetManager::GetDefaultMaterialInstance();
            }

            TRef<FTexture2D> lightmapTex;
            bool bUseLM = World->AreLightmapsTrusted() && mesh.Mobility == EComponentMobility::Static &&
                          mesh.LightmapIndex >= 0 && !mesh.LightmapAssetPath.empty();
            if (bUseLM) {
                auto lm = UAssetManager::GetLightmap(mesh.LightmapAssetPath);
                if (lm)
                    lightmapTex = lm->GetOrCreateGPUTexture();
            }

            if (matInst->GetAlphaMode() == EAlphaMode::Blend) {
                FTransparentDraw draw;
                draw.Shader = mesh.Shader;
                draw.VA = mesh.VertexArray;
                draw.Mat = matInst;
                draw.Model = model;
                glm::vec3 e = ProceduralMeshLocalExtent(mesh);
                draw.DistanceSq = TransparentSortDistanceSq(model, -e, e, camPos);
                draw.bReceiveShadows = mesh.bReceiveShadows;
                draw.bUseLightmap = bUseLM;
                draw.bLightmapUseTexCoord = false;
                draw.LightmapScale = mesh.LightmapScale;
                draw.LightmapBias = mesh.LightmapBias;
                draw.Lightmap = lightmapTex;
                GPendingTransparents.push_back(std::move(draw));
                continue;
            }

            FOpaqueDraw draw;
            draw.Shader = mesh.Shader;
            draw.VA = mesh.VertexArray;
            draw.Mat = matInst;
            draw.Model = model;
            draw.bReceiveShadows = mesh.bReceiveShadows;
            draw.bUseLightmap = bUseLM;
            draw.bPlanar = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
            draw.LightmapScale = mesh.LightmapScale;
            draw.LightmapBias = mesh.LightmapBias;
            draw.Lightmap = lightmapTex;
            SubmitOpaqueDraw(std::move(draw));
        }

        // Static Mesh Component Rendering
        auto staticMeshView = reg.view<FTransformComponent, FStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, FStaticMeshComponent>(entity);
            if (!staticMeshComp.bVisible || !staticMeshComp.StaticMesh || !staticMeshComp.StaticMesh->GetVertexArray())
                continue;
            glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
            if (IsStaticMeshCulled(world, staticMeshComp, camFrustum))
                continue;

            const uint32_t lod = UpdateStaticMeshLOD(staticMeshComp, world, &InCamera);
            auto lodVA = staticMeshComp.StaticMesh->GetLODVertexArray(lod);
            if (!lodVA)
                continue;
            const auto& submeshes = staticMeshComp.StaticMesh->GetLODSubmeshes(lod);
            RecordStaticMeshLODStats(lod, staticMeshComp.StaticMesh->GetSourceTriangleCount(),
                                     staticMeshComp.StaticMesh->GetLODTriangleCount(lod));

            TRef<FShader> activeShader = staticMeshComp.Shader
                                             ? staticMeshComp.Shader
                                             : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!activeShader)
                continue;

            for (const auto& submesh : submeshes) {
                if (submesh.IndexCount == 0)
                    continue;

                TRef<FMaterialInstance> matInst =
                    ResolveStaticSubmeshMaterial(*staticMeshComp.StaticMesh, submesh, staticMeshComp.MaterialOverrides);

                glm::mat4 model = world * submesh.LocalTransform;

                TRef<FTexture2D> lightmapTex;
                bool bUseLM = World->AreLightmapsTrusted() &&
                              staticMeshComp.Mobility == EComponentMobility::Static &&
                              staticMeshComp.LightmapIndex >= 0 && !staticMeshComp.LightmapAssetPath.empty();
                if (bUseLM) {
                    auto lm = UAssetManager::GetLightmap(staticMeshComp.LightmapAssetPath);
                    if (lm)
                        lightmapTex = lm->GetOrCreateGPUTexture();
                }

                if (matInst->GetAlphaMode() == EAlphaMode::Blend) {
                    FTransparentDraw draw;
                    draw.Shader = activeShader;
                    draw.VA = lodVA;
                    draw.Mat = matInst;
                    draw.Model = model;
                    draw.DistanceSq = TransparentSortDistanceSq(model, staticMeshComp.StaticMesh->GetBoundsMin(),
                                                                staticMeshComp.StaticMesh->GetBoundsMax(), camPos);
                    draw.IndexCount = submesh.IndexCount;
                    draw.IndexOffset = submesh.IndexOffset;
                    draw.bOffset = true;
                    draw.bReceiveShadows = staticMeshComp.bReceiveShadows;
                    draw.bUseLightmap = bUseLM;
                    draw.LightmapScale = staticMeshComp.LightmapScale;
                    draw.LightmapBias = staticMeshComp.LightmapBias;
                    draw.Lightmap = lightmapTex;
                    GPendingTransparents.push_back(std::move(draw));
                    continue;
                }

                FOpaqueDraw draw;
                draw.Shader = activeShader;
                draw.VA = lodVA;
                draw.Mat = matInst;
                draw.Model = model;
                draw.IndexCount = submesh.IndexCount;
                draw.IndexOffset = submesh.IndexOffset;
                draw.bOffset = true;
                draw.bReceiveShadows = staticMeshComp.bReceiveShadows;
                draw.bUseLightmap = bUseLM;
                draw.bPlanar = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
                draw.LightmapScale = staticMeshComp.LightmapScale;
                draw.LightmapBias = staticMeshComp.LightmapBias;
                draw.Lightmap = lightmapTex;
                SubmitOpaqueDraw(std::move(draw));
            }
        }

        auto skelGeomView = reg.view<FTransformComponent, FSkinnedMeshRenderState>();
        for (auto entity : skelGeomView) {
            auto [transform, skel] = skelGeomView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
            if (!skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray())
                continue;
            if (!skel.bVisible)
                continue;
            glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
            if (IsSkeletalMeshCulled(world, skel, camFrustum))
                continue;

            TRef<FShader> activeShader =
                skel.Shader ? skel.Shader : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Skinned.glsl");
            if (!activeShader)
                continue;

            for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;
                TRef<FMaterialInstance> matInst =
                    ResolveSkeletalSubmeshMaterial(*skel.SkeletalMesh, submesh, skel.MaterialOverrides);
                glm::mat4 model = SkeletalModelMatrix(world, skel, submesh.LocalTransform);
                if (matInst->GetAlphaMode() == EAlphaMode::Blend) {
                    FTransparentDraw draw;
                    draw.Shader = activeShader;
                    draw.VA = skel.SkeletalMesh->GetVertexArray();
                    draw.Mat = matInst;
                    draw.Model = model;
                    draw.DistanceSq = TransparentSortDistanceSq(model, skel.SkeletalMesh->GetBoundsMin(),
                                                                skel.SkeletalMesh->GetBoundsMax(), camPos);
                    draw.IndexCount = submesh.IndexCount;
                    draw.IndexOffset = submesh.IndexOffset;
                    draw.bOffset = true;
                    draw.bReceiveShadows = skel.bReceiveShadows;
                    draw.BonePalette = &skel.BonePalette;
                    GPendingTransparents.push_back(std::move(draw));
                    continue;
                }
                FOpaqueDraw draw;
                draw.Shader = activeShader;
                draw.VA = skel.SkeletalMesh->GetVertexArray();
                draw.Mat = matInst;
                draw.Model = model;
                draw.IndexCount = submesh.IndexCount;
                draw.IndexOffset = submesh.IndexOffset;
                draw.bOffset = true;
                draw.bReceiveShadows = skel.bReceiveShadows;
                draw.bPlanar = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
                draw.bSkinned = true;
                draw.BonePalette = &skel.BonePalette;
                SubmitOpaqueDraw(std::move(draw));
            }
        }

        std::sort(GPendingOpaques.begin(), GPendingOpaques.end(), [](const FOpaqueDraw& a, const FOpaqueDraw& b) {
            if (a.bSkinned != b.bSkinned)
                return a.bSkinned < b.bSkinned;
            if (a.Shader != b.Shader)
                return a.Shader.get() < b.Shader.get();
            if (a.VA != b.VA)
                return a.VA.get() < b.VA.get();
            if (a.Mat != b.Mat)
                return a.Mat.get() < b.Mat.get();
            if (a.IndexOffset != b.IndexOffset)
                return a.IndexOffset < b.IndexOffset;
            return a.IndexCount < b.IndexCount;
        });

        FShader* lastShader = nullptr;
        FMaterialInstance* lastMat = nullptr;
        FVertexArray* lastVA = nullptr;
        const std::vector<glm::mat4>* lastBones = nullptr;
        bool bLastShadows = false;
        bool bLastSpotShadows = false;
        bool bLastPointShadows = false;
        bool bLastPlanar = false;
        alignas(16) glm::mat4 instanceMats[kMaxOpaqueInstances];

        auto bindPassGlobals = [&](FOpaqueDraw& draw) {
            bool bShaderChanged = draw.Shader.get() != lastShader;
            if (bShaderChanged) {
                draw.Shader->Bind();
                lastShader = draw.Shader.get();
                lastMat = nullptr;
                lastBones = nullptr;
                draw.Shader->SetInt("u_EnableClipPlane", 0);
                draw.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
                draw.Shader->SetInt("u_DebugMode", DebugMode);
            }
            bool bWantShadows = bShadowsAvailable && draw.bReceiveShadows;
            bool bWantSpot = bSpotShadowAvailable && draw.bReceiveShadows;
            bool bWantPoint = bPointShadowAvailable && draw.bReceiveShadows;
            if (bShaderChanged || bWantShadows != bLastShadows) {
                draw.Shader->SetInt("u_UseShadows", bWantShadows ? 1 : 0);
                bLastShadows = bWantShadows;
            }
            if (bShaderChanged || bWantSpot != bLastSpotShadows) {
                draw.Shader->SetInt("u_UseSpotShadows", bWantSpot ? 1 : 0);
                bLastSpotShadows = bWantSpot;
            }
            if (bShaderChanged || bWantPoint != bLastPointShadows) {
                draw.Shader->SetInt("u_UsePointShadows", bWantPoint ? 1 : 0);
                bLastPointShadows = bWantPoint;
            }
            if (bShaderChanged || draw.bPlanar != bLastPlanar) {
                BindPlanarReflectionUniforms(*draw.Shader, draw.bPlanar);
                bLastPlanar = draw.bPlanar;
            }
            if (draw.BonePalette && draw.BonePalette != lastBones) {
                UploadBonePalette(BonePaletteUBO.get(), *draw.BonePalette);
                lastBones = draw.BonePalette;
            }
            if (draw.Mat.get() != lastMat) {
                draw.Mat->Bind(draw.Shader);
                lastMat = draw.Mat.get();
            }
            BindLightmapUniforms(*draw.Shader, draw.bUseLightmap, draw.bLightmapUseTexCoord, draw.LightmapScale,
                                 draw.LightmapBias, draw.Lightmap);
            if (draw.VA.get() != lastVA) {
                draw.VA->Bind();
                lastVA = draw.VA.get();
            }
        };

        for (size_t i = 0; i < GPendingOpaques.size();) {
            FOpaqueDraw& first = GPendingOpaques[i];
            if (!first.Shader || !first.VA || !first.Mat) {
                ++i;
                continue;
            }

            size_t batchEnd = i + 1;
            if (!first.bSkinned) {
                while (batchEnd < GPendingOpaques.size() && (batchEnd - i) < kMaxOpaqueInstances &&
                       OpaqueDrawsBatchable(first, GPendingOpaques[batchEnd]))
                    ++batchEnd;
            }
            const uint32_t instanceCount = static_cast<uint32_t>(batchEnd - i);
            bindPassGlobals(first);
            ApplyMeshRasterState(*first.Mat, first.Model, false);

            if (instanceCount == 1) {
                first.Shader->SetInt("u_UseInstancing", 0);
                first.Shader->SetMat4("u_Model", glm::value_ptr(first.Model));
                glm::mat3 normalMatrix = SafeNormalMatrix(first.Model);
                first.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                if (first.bOffset)
                    FRenderCommand::DrawIndexedOffset(first.VA, first.IndexCount, first.IndexOffset);
                else
                    FRenderCommand::DrawIndexed(first.VA);
            } else {
                for (uint32_t n = 0; n < instanceCount; ++n)
                    instanceMats[n] = GPendingOpaques[i + n].Model;
                if (InstanceUBO)
                    InstanceUBO->SetData(instanceMats, static_cast<unsigned int>(sizeof(glm::mat4) * instanceCount), 0);
                first.Shader->SetInt("u_UseInstancing", 1);
                if (first.bOffset)
                    FRenderCommand::DrawIndexedOffsetInstanced(first.VA, first.IndexCount, first.IndexOffset,
                                                               instanceCount);
                else
                    FRenderCommand::DrawIndexedInstanced(first.VA, 0, instanceCount);
            }
            i = batchEnd;
        }
        GPendingOpaques.clear();

        // Friend/foe silhouette: inverted-hull (expand along normals, cull front faces).
        if (TRef<FShader> outlineShader = UAssetManager::GetShader("Engine/Assets/Shaders/Outline_Skinned.glsl")) {
            FRenderCommand::SetCulling(true, ECullMode::Front);
            FRenderCommand::SetDepthMask(false);
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetBlendState(false);
            for (auto entity : skelGeomView) {
                auto [transform, skel] = skelGeomView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
                if (!skel.bDrawOutline || !skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray())
                    continue;
                if (!skel.bVisible)
                    continue;
                glm::mat4 world = ResolveActorWorldMatrix(World, entity, transform);
                if (IsSkeletalMeshCulled(world, skel, camFrustum))
                    continue;
                UploadBonePalette(BonePaletteUBO.get(), skel.BonePalette);
                outlineShader->Bind();
                outlineShader->SetFloat3("u_OutlineColor", skel.OutlineColor.x, skel.OutlineColor.y,
                                         skel.OutlineColor.z);
                outlineShader->SetFloat("u_OutlineWidth", skel.OutlineWidth);
                skel.SkeletalMesh->GetVertexArray()->Bind();
                for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                    if (submesh.IndexCount == 0)
                        continue;
                    glm::mat4 model = SkeletalModelMatrix(world, skel, submesh.LocalTransform);
                    outlineShader->SetMat4("u_Model", glm::value_ptr(model));
                    glm::mat3 normalMatrix = SafeNormalMatrix(model);
                    outlineShader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                    FRenderCommand::DrawIndexedOffset(skel.SkeletalMesh->GetVertexArray(), submesh.IndexCount,
                                                      submesh.IndexOffset);
                }
            }
            FRenderCommand::SetDepthMask(true);
            FRenderCommand::SetDepthFunc(EDepthFunc::Less);
            FRenderCommand::SetCulling(true, ECullMode::Back);
        }

        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        FRenderCommand::SetClipDistance(false);
    }

    void FWorldRenderer::RenderTransparentGeometryPass(bool bHasDirLight, bool bHasSpotLight) {
        if (GPendingTransparents.empty())
            return;

        if (CascadeShadowFramebuffer)
            CascadeShadowFramebuffer->BindDepthTexture(10);
        if (SpotShadowFramebuffer)
            SpotShadowFramebuffer->BindDepthTexture(11);
        if (PointShadowFramebuffer)
            PointShadowFramebuffer->BindDepthTexture(14);

        bool bShadowsOn = ShadowSettings.bEnableShadows;
        bool bShadowsAvailable = bShadowsOn && bHasDirLight && CascadeShadowFramebuffer;
        bool bSpotShadowAvailable = bShadowsOn && bHasSpotLight && SpotShadowFramebuffer;
        bool bPointShadowAvailable = bShadowsOn && PointShadowFramebuffer;
        bool bIBLAvailable = bUseIBL && IBLEnvironment.BRDFLUT;
        if (bIBLAvailable) {
            if (IBLEnvironment.BRDFLUT)
                IBLEnvironment.BRDFLUT->Bind(6);
            if (IBLEnvironment.IrradianceMap)
                IBLEnvironment.IrradianceMap->Bind(7);
            if (IBLEnvironment.PrefilterMap)
                IBLEnvironment.PrefilterMap->Bind(8);
        }

        std::sort(GPendingTransparents.begin(), GPendingTransparents.end(),
                  [](const FTransparentDraw& a, const FTransparentDraw& b) { return a.DistanceSq > b.DistanceSq; });
        for (const auto& draw : GPendingTransparents) {
            if (!draw.Shader || !draw.VA || !draw.Mat)
                continue;
            if (draw.BonePalette)
                UploadBonePalette(BonePaletteUBO.get(), *draw.BonePalette);
            draw.Shader->Bind();
            draw.Shader->SetInt("u_EnableClipPlane", 0);
            draw.Shader->SetInt("u_UseInstancing", 0);
            draw.Shader->SetInt("u_UseShadows", (bShadowsAvailable && draw.bReceiveShadows) ? 1 : 0);
            draw.Shader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && draw.bReceiveShadows) ? 1 : 0);
            draw.Shader->SetInt("u_UsePointShadows", (bPointShadowAvailable && draw.bReceiveShadows) ? 1 : 0);
            draw.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            draw.Shader->SetInt("u_DebugMode", DebugMode);
            BindPlanarReflectionUniforms(*draw.Shader,
                                         draw.Mat->GetUsePlanarReflection() && PlanarReflectionFramebuffer);
            ApplyMeshRasterState(*draw.Mat, draw.Model, true);
            draw.Mat->Bind(draw.Shader);
            BindLightmapUniforms(*draw.Shader, draw.bUseLightmap, draw.bLightmapUseTexCoord, draw.LightmapScale,
                                 draw.LightmapBias, draw.Lightmap);
            draw.Shader->SetMat4("u_Model", glm::value_ptr(draw.Model));
            glm::mat3 normalMatrix = SafeNormalMatrix(draw.Model);
            draw.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
            draw.VA->Bind();
            if (draw.bOffset)
                FRenderCommand::DrawIndexedOffset(draw.VA, draw.IndexCount, draw.IndexOffset);
            else
                FRenderCommand::DrawIndexed(draw.VA);
        }

        GPendingTransparents.clear();
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        FRenderCommand::SetClipDistance(false);
    }

} // namespace Leon
