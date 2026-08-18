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

    void FWorldRenderer::BindPlanarReflectionUniforms(FShader& InShader, bool bEnabled) {
        if (bEnabled && PlanarReflectionFramebuffer) {
            PlanarReflectionFramebuffer->BindTexture(0, 5);
            InShader.SetInt("u_UsePlanarReflection", 1);
            InShader.SetMat4("u_PlanarViewProjection", glm::value_ptr(PlanarViewProjection));
            InShader.SetFloat3("u_PlanarPlaneNormal", PlanarPlaneNormal.x, PlanarPlaneNormal.y, PlanarPlaneNormal.z);
        } else {
            InShader.SetInt("u_UsePlanarReflection", 0);
        }
        if (bEnabled && bWallPlanarActive && WallPlanarReflectionFramebuffer) {
            WallPlanarReflectionFramebuffer->BindTexture(0, 13);
            InShader.SetInt("u_UsePlanarReflection1", 1);
            InShader.SetMat4("u_PlanarViewProjection1", glm::value_ptr(WallPlanarViewProjection));
            InShader.SetFloat3("u_PlanarPlaneNormal1", WallPlanarPlaneNormal.x, WallPlanarPlaneNormal.y,
                               WallPlanarPlaneNormal.z);
        } else {
            if (DefaultBlackTexture)
                DefaultBlackTexture->Bind(13);
            InShader.SetInt("u_UsePlanarReflection1", 0);
        }
    }

    // =========================================================================
    // PASS 4: Geometry Pass
    // Audit fix ALTO-01: shadow maps + IBL bound ONCE before the loop, not per-object
    // Audit fix MEDIO-05: normal matrix calculated CPU-side, uploaded as u_NormalMatrix
    // =========================================================================
    void FWorldRenderer::RenderGeometryPass(const FPerspectiveCamera& InCamera, bool bHasDirLight, bool bHasSpotLight,
                                            uint32_t InVpWidth, uint32_t InVpHeight) {
        (void)InVpWidth;
        (void)InVpHeight;
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
        std::vector<FTransparentDraw> transparents;
        glm::vec3 camPos = InCamera.GetPosition();

        for (auto entity : meshView) {
            auto [transform, mesh] = meshView.get<FTransformComponent, FMeshComponent>(entity);
            if (!mesh.VertexArray || !mesh.Shader || !mesh.bVisible)
                continue;

            if (IsProceduralMeshCulled(transform, mesh, camFrustum))
                continue;

            TRef<FMaterialInstance> matInst = nullptr;
            if (reg.all_of<FMaterialComponent>(entity)) {
                matInst = reg.get<FMaterialComponent>(entity).MaterialInstance;
            }
            if (!matInst) {
                matInst = UAssetManager::GetDefaultMaterial()->CreateInstance();
            }

            glm::mat4 model = transform.GetTransform();
            TRef<FTexture2D> lightmapTex;
            bool bUseLM = mesh.Mobility == EComponentMobility::Static && mesh.LightmapIndex >= 0 &&
                          !mesh.LightmapAssetPath.empty();
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
                glm::vec3 delta = glm::vec3(model[3]) - camPos;
                draw.DistanceSq = glm::dot(delta, delta);
                draw.bReceiveShadows = mesh.bReceiveShadows;
                draw.bUseLightmap = bUseLM;
                draw.bLightmapUseTexCoord = false;
                draw.LightmapScale = mesh.LightmapScale;
                draw.LightmapBias = mesh.LightmapBias;
                draw.Lightmap = lightmapTex;
                transparents.push_back(std::move(draw));
                continue;
            }

            mesh.Shader->Bind();
            mesh.Shader->SetInt("u_EnableClipPlane", 0);
            mesh.Shader->SetInt("u_UseShadows", (bShadowsAvailable && mesh.bReceiveShadows) ? 1 : 0);
            mesh.Shader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && mesh.bReceiveShadows) ? 1 : 0);

            bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
            BindPlanarReflectionUniforms(*mesh.Shader, bApplyPlanarReflection);

            ApplyMeshRasterState(*matInst, model, false);
            matInst->Bind(mesh.Shader);
            BindLightmapUniforms(*mesh.Shader, bUseLM, false, mesh.LightmapScale, mesh.LightmapBias, lightmapTex);
            mesh.Shader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            mesh.Shader->SetInt("u_DebugMode", DebugMode);
            mesh.Shader->SetMat4("u_Model", glm::value_ptr(model));
            glm::mat3 normalMatrix = SafeNormalMatrix(model);
            mesh.Shader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
            mesh.VertexArray->Bind();
            FRenderCommand::DrawIndexed(mesh.VertexArray);
        }

        // Static Mesh Component Rendering
        auto staticMeshView = reg.view<FTransformComponent, FStaticMeshComponent>();
        for (auto entity : staticMeshView) {
            auto [transform, staticMeshComp] = staticMeshView.get<FTransformComponent, FStaticMeshComponent>(entity);
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

                glm::mat4 model = transform.GetTransform() * submesh.LocalTransform;

                TRef<FTexture2D> lightmapTex;
                bool bUseLM = staticMeshComp.Mobility == EComponentMobility::Static &&
                              staticMeshComp.LightmapIndex >= 0 && !staticMeshComp.LightmapAssetPath.empty();
                if (bUseLM) {
                    auto lm = UAssetManager::GetLightmap(staticMeshComp.LightmapAssetPath);
                    if (lm)
                        lightmapTex = lm->GetOrCreateGPUTexture();
                }

                if (matInst->GetAlphaMode() == EAlphaMode::Blend) {
                    FTransparentDraw draw;
                    draw.Shader = activeShader;
                    draw.VA = staticMeshComp.StaticMesh->GetVertexArray();
                    draw.Mat = matInst;
                    draw.Model = model;
                    glm::vec3 delta = glm::vec3(model[3]) - camPos;
                    draw.DistanceSq = glm::dot(delta, delta);
                    draw.IndexCount = submesh.IndexCount;
                    draw.IndexOffset = submesh.IndexOffset;
                    draw.bOffset = true;
                    draw.bReceiveShadows = staticMeshComp.bReceiveShadows;
                    draw.bUseLightmap = bUseLM;
                    draw.LightmapScale = staticMeshComp.LightmapScale;
                    draw.LightmapBias = staticMeshComp.LightmapBias;
                    draw.Lightmap = lightmapTex;
                    transparents.push_back(std::move(draw));
                    continue;
                }

                bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
                BindPlanarReflectionUniforms(*activeShader, bApplyPlanarReflection);

                ApplyMeshRasterState(*matInst, model, false);
                matInst->Bind(activeShader);
                BindLightmapUniforms(*activeShader, bUseLM, false, staticMeshComp.LightmapScale,
                                     staticMeshComp.LightmapBias, lightmapTex);

                activeShader->SetInt("u_EnableClipPlane", 0);
                activeShader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = SafeNormalMatrix(model);
                activeShader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

                FRenderCommand::DrawIndexedOffset(staticMeshComp.StaticMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

        auto skelGeomView = reg.view<FTransformComponent, FSkinnedMeshRenderState>();
        for (auto entity : skelGeomView) {
            auto [transform, skel] = skelGeomView.get<FTransformComponent, FSkinnedMeshRenderState>(entity);
            if (!skel.SkeletalMesh || !skel.SkeletalMesh->GetVertexArray())
                continue;
            if (!skel.bVisible)
                continue;
            if (IsSkeletalMeshCulled(transform, skel, camFrustum))
                continue;

            TRef<FShader> activeShader =
                skel.Shader ? skel.Shader : UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Skinned.glsl");
            if (!activeShader)
                continue;
            UploadBonePalette(BonePaletteUBO.get(), skel.BonePalette);
            activeShader->Bind();
            activeShader->SetInt("u_UseShadows", (bShadowsAvailable && skel.bReceiveShadows) ? 1 : 0);
            activeShader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && skel.bReceiveShadows) ? 1 : 0);
            activeShader->SetInt("u_UseIBL", bIBLAvailable ? 1 : 0);
            activeShader->SetInt("u_DebugMode", DebugMode);
            skel.SkeletalMesh->GetVertexArray()->Bind();

            for (const auto& submesh : skel.SkeletalMesh->GetSubmeshes()) {
                if (submesh.IndexCount == 0)
                    continue;
                TRef<FMaterialInstance> matInst =
                    ResolveSkeletalSubmeshMaterial(*skel.SkeletalMesh, submesh, skel.MaterialOverrides);
                glm::mat4 model = SkeletalModelMatrix(transform, skel, submesh.LocalTransform);
                if (matInst->GetAlphaMode() == EAlphaMode::Blend) {
                    FTransparentDraw draw;
                    draw.Shader = activeShader;
                    draw.VA = skel.SkeletalMesh->GetVertexArray();
                    draw.Mat = matInst;
                    draw.Model = model;
                    glm::vec3 delta = glm::vec3(model[3]) - camPos;
                    draw.DistanceSq = glm::dot(delta, delta);
                    draw.IndexCount = submesh.IndexCount;
                    draw.IndexOffset = submesh.IndexOffset;
                    draw.bOffset = true;
                    draw.bReceiveShadows = skel.bReceiveShadows;
                    draw.BonePalette = &skel.BonePalette;
                    transparents.push_back(std::move(draw));
                    continue;
                }
                bool bApplyPlanarReflection = matInst->GetUsePlanarReflection() && PlanarReflectionFramebuffer;
                BindPlanarReflectionUniforms(*activeShader, bApplyPlanarReflection);
                ApplyMeshRasterState(*matInst, model, false);
                matInst->Bind(activeShader);
                BindLightmapUniforms(*activeShader, false, false, glm::vec2(1.0f), glm::vec2(0.0f), nullptr);
                activeShader->SetInt("u_EnableClipPlane", 0);
                activeShader->SetMat4("u_Model", glm::value_ptr(model));
                glm::mat3 normalMatrix = SafeNormalMatrix(model);
                activeShader->SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));
                FRenderCommand::DrawIndexedOffset(skel.SkeletalMesh->GetVertexArray(), submesh.IndexCount,
                                                  submesh.IndexOffset);
            }
        }

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
                if (IsSkeletalMeshCulled(transform, skel, camFrustum))
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
                    glm::mat4 model = SkeletalModelMatrix(transform, skel, submesh.LocalTransform);
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

        std::sort(transparents.begin(), transparents.end(),
                  [](const FTransparentDraw& a, const FTransparentDraw& b) { return a.DistanceSq > b.DistanceSq; });
        for (const auto& draw : transparents) {
            if (!draw.Shader || !draw.VA || !draw.Mat)
                continue;
            if (draw.BonePalette)
                UploadBonePalette(BonePaletteUBO.get(), *draw.BonePalette);
            draw.Shader->Bind();
            draw.Shader->SetInt("u_EnableClipPlane", 0);
            draw.Shader->SetInt("u_UseShadows", (bShadowsAvailable && draw.bReceiveShadows) ? 1 : 0);
            draw.Shader->SetInt("u_UseSpotShadows", (bSpotShadowAvailable && draw.bReceiveShadows) ? 1 : 0);
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

        // Restore pass-level default rasterizer state
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        FRenderCommand::SetClipDistance(false);
    }

} // namespace Leon
