#pragma once

#include "Assets/FAnimTypes.hpp"
#include "Assets/FLODSettings.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FRenderer.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"
#include "Renderer/FFrustumCull.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Renderer/FShadowTypes.hpp"
#include "RHI/FTexture.hpp"
#include "Core/FFrameProfiler.hpp"

#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

namespace Leon {

    constexpr uint32_t kMaxOpaqueInstances = 64;

    inline float MaxAffineScale(const glm::mat4& InMatrix) {
        return std::max({glm::length(glm::vec3(InMatrix[0])), glm::length(glm::vec3(InMatrix[1])),
                         glm::length(glm::vec3(InMatrix[2])), 1.0e-4f});
    }

    inline uint32_t UpdateStaticMeshLOD(FStaticMeshComponent& InComp, const glm::mat4& InWorld,
                                        const FPerspectiveCamera* InCamera) {
        if (!InComp.StaticMesh)
            return 0;
        const uint32_t count = std::max(1u, InComp.StaticMesh->GetLODCount());
        if (InComp.ForcedLOD != kForcedLODAuto) {
            InComp.CurrentLOD = std::min(InComp.ForcedLOD, count - 1);
            return InComp.CurrentLOD;
        }
        if (!InCamera) {
            InComp.CurrentLOD = std::min(InComp.CurrentLOD, count - 1);
            return InComp.CurrentLOD;
        }
        const glm::vec3 center = glm::vec3(InWorld * glm::vec4(InComp.StaticMesh->GetSphereCenter(), 1.0f));
        const float radius = InComp.StaticMesh->GetSphereRadius() * MaxAffineScale(InWorld);
        const float screen =
            ComputeProjectedScreenHeight(center, radius, InCamera->GetPosition(), InCamera->GetFOV());
        InComp.CurrentLOD = SelectStaticMeshLOD(screen, InComp.CurrentLOD, count);
        return InComp.CurrentLOD;
    }

    inline uint32_t ShadowLODIndex(const FStaticMeshComponent& InComp, uint32_t InSelected) {
        const uint32_t count = InComp.StaticMesh ? std::max(1u, InComp.StaticMesh->GetLODCount()) : 1u;
        if (!InComp.bUseCoarserShadowLOD)
            return std::min(InSelected, count - 1);
        return std::min(InSelected + 1u, count - 1);
    }

    inline void RecordStaticMeshLODStats(uint32_t InLOD, uint32_t InSourceTris, uint32_t InSubmittedTris) {
        auto& stats = FRenderer::GetStatsMutable();
        if (InLOD < kRenderStatLODSlots)
            stats.StaticMeshLODCounts[InLOD]++;
        stats.StaticMeshSourceTriangles += InSourceTris;
        stats.StaticMeshSubmittedTriangles += InSubmittedTris;
    }

    struct FGpuCpuScope {
        FFrameProfiler::FScope Cpu;
        uint32_t Slot = 0;
        FGpuCpuScope(float* InCpuMs, uint32_t InSlot) : Cpu(InCpuMs), Slot(InSlot) {
            FRenderCommand::BeginGPUTimeQuery(InSlot);
        }
        ~FGpuCpuScope() { FRenderCommand::EndGPUTimeQuery(Slot); }
    };

    inline glm::mat4 ResolveActorWorldMatrix(UWorld* InWorld, entt::entity InEntity,
                                             const FTransformComponent& InTransform) {
        if (InWorld) {
            if (AActor* actor = InWorld->FindActorByEntity(InEntity))
                return actor->GetActorWorldMatrix();
        }
        return InTransform.GetTransform();
    }

    inline ECullMode FlipCullForNegativeScale(ECullMode InMode, const glm::mat4& InModel) {
        if (InMode == ECullMode::None || !HasNegativeScale(InModel))
            return InMode;
        if (InMode == ECullMode::Back)
            return ECullMode::Front;
        if (InMode == ECullMode::Front)
            return ECullMode::Back;
        return InMode;
    }

    inline void ApplyMeshRasterState(FMaterialInstance& InMat, const glm::mat4& InModel, bool bTransparent) {
        const auto& pso = InMat.GetPipelineState();
        ECullMode cullMode = InMat.GetDoubleSided() ? ECullMode::None : pso.CullMode;
        cullMode = FlipCullForNegativeScale(cullMode, InModel);
        FRenderCommand::SetCulling(cullMode != ECullMode::None, cullMode);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(!bTransparent);
        FRenderCommand::SetDepthFunc(pso.DepthFunc);
        FRenderCommand::SetBlendState(bTransparent);
        if (bTransparent)
            FRenderCommand::SetBlendFunc(pso.SrcBlend, pso.DstBlend);
    }

    /** Default mesh raster after UI/debug/sky/shadow side-effects. Call at CSM/spot start and HDR bind. */
    inline void ResetDefaultMeshRasterState() {
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetPolygonOffset(false);
        FRenderCommand::SetClipDistance(false);
        FRenderCommand::SetLineWidth(1.0f);
    }

    struct FTransparentDraw {
        TRef<FShader> Shader;
        TRef<FVertexArray> VA;
        TRef<FMaterialInstance> Mat;
        glm::mat4 Model{1.0f};
        float DistanceSq = 0.0f;
        uint32_t IndexCount = 0;
        uint32_t IndexOffset = 0;
        bool bOffset = false;
        bool bReceiveShadows = true;
        bool bUseLightmap = false;
        bool bLightmapUseTexCoord = false;
        glm::vec2 LightmapScale{1.0f};
        glm::vec2 LightmapBias{0.0f};
        TRef<FTexture2D> Lightmap;
        const std::vector<glm::mat4>* BonePalette = nullptr;
    };

    struct FOpaqueDraw {
        TRef<FShader> Shader;
        TRef<FVertexArray> VA;
        TRef<FMaterialInstance> Mat;
        glm::mat4 Model{1.0f};
        uint32_t IndexCount = 0;
        uint32_t IndexOffset = 0;
        bool bOffset = false;
        bool bReceiveShadows = true;
        bool bUseLightmap = false;
        bool bLightmapUseTexCoord = false;
        bool bPlanar = false;
        bool bSkinned = false;
        glm::vec2 LightmapScale{1.0f};
        glm::vec2 LightmapBias{0.0f};
        TRef<FTexture2D> Lightmap;
        const std::vector<glm::mat4>* BonePalette = nullptr;
    };

    inline bool OpaqueDrawsBatchable(const FOpaqueDraw& InA, const FOpaqueDraw& InB) {
        if (InA.bSkinned || InB.bSkinned || InA.BonePalette || InB.BonePalette)
            return false;
        if (InA.Shader != InB.Shader || InA.VA != InB.VA || InA.Mat != InB.Mat)
            return false;
        if (InA.IndexCount != InB.IndexCount || InA.IndexOffset != InB.IndexOffset || InA.bOffset != InB.bOffset)
            return false;
        if (InA.bReceiveShadows != InB.bReceiveShadows || InA.bUseLightmap != InB.bUseLightmap)
            return false;
        if (InA.bLightmapUseTexCoord != InB.bLightmapUseTexCoord || InA.Lightmap != InB.Lightmap)
            return false;
        if (InA.bPlanar != InB.bPlanar)
            return false;
        if (InA.LightmapScale != InB.LightmapScale || InA.LightmapBias != InB.LightmapBias)
            return false;
        return HasNegativeScale(InA.Model) == HasNegativeScale(InB.Model);
    }

    inline bool IsStaticMeshCulled(const glm::mat4& InWorld, const FStaticMeshComponent& InMesh,
                            const FFrustumPlanes& InFrustum) {
        if (!InMesh.bVisible)
            return true;
        if (!InMesh.StaticMesh)
            return false;
        glm::vec3 wMin, wMax;
        TransformAABB(InMesh.StaticMesh->GetBoundsMin(), InMesh.StaticMesh->GetBoundsMax(), InWorld, wMin, wMax);
        if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
            FRenderer::GetStatsMutable().MeshesCulled++;
            return true;
        }
        FRenderer::GetStatsMutable().MeshesDrawn++;
        return false;
    }

    inline bool IsSkeletalMeshCulled(const glm::mat4& InWorld, const FSkinnedMeshRenderState& InMesh,
                              const FFrustumPlanes& InFrustum) {
        if (!InMesh.bVisible)
            return true;
        if (!InMesh.SkeletalMesh)
            return false;
        glm::mat4 relative = glm::translate(glm::mat4(1.0f), InMesh.RelativeLocation) *
                             glm::toMat4(glm::quat(glm::radians(InMesh.RelativeRotation))) *
                             glm::scale(glm::mat4(1.0f), InMesh.RelativeScale);
        glm::vec3 wMin, wMax;
        TransformAABB(InMesh.SkeletalMesh->GetBoundsMin(), InMesh.SkeletalMesh->GetBoundsMax(), InWorld * relative,
                      wMin, wMax);
        if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
            FRenderer::GetStatsMutable().MeshesCulled++;
            return true;
        }
        FRenderer::GetStatsMutable().MeshesDrawn++;
        return false;
    }

    inline glm::mat4 SkeletalModelMatrix(const glm::mat4& InWorld, const FSkinnedMeshRenderState& InMesh,
                                  const glm::mat4& InSubmeshLocal) {
        glm::mat4 relative = glm::translate(glm::mat4(1.0f), InMesh.RelativeLocation) *
                             glm::toMat4(glm::quat(glm::radians(InMesh.RelativeRotation))) *
                             glm::scale(glm::mat4(1.0f), InMesh.RelativeScale);
        return InWorld * relative * InSubmeshLocal;
    }

    inline void UploadBonePalette(FUniformBuffer* InUBO, const std::vector<glm::mat4>& InPalette) {
        if (!InUBO)
            return;
        alignas(16) glm::mat4 padded[kMaxBones];
        for (uint32_t i = 0; i < kMaxBones; ++i)
            padded[i] = (i < InPalette.size()) ? InPalette[i] : glm::mat4(1.0f);
        InUBO->SetData(padded, sizeof(padded), 0);
    }

    inline glm::vec3 ProceduralMeshLocalExtent(const FMeshComponent& InMesh) {
        float e = std::max({InMesh.MeshSize * 0.5f, InMesh.MeshRadius, InMesh.MeshWidth * 0.5f,
                            InMesh.MeshHeight * 0.5f, InMesh.MeshDepth * 0.5f, 0.5f});
        return glm::vec3(e);
    }

    inline bool IsProceduralMeshCulled(const glm::mat4& InWorld, const FMeshComponent& InMesh,
                                const FFrustumPlanes& InFrustum) {
        glm::vec3 e = ProceduralMeshLocalExtent(InMesh);
        glm::vec3 localMin(-e), localMax(e);
        glm::vec3 wMin, wMax;
        TransformAABB(localMin, localMax, InWorld, wMin, wMax);
        if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
            FRenderer::GetStatsMutable().MeshesCulled++;
            return true;
        }
        FRenderer::GetStatsMutable().MeshesDrawn++;
        return false;
    }

    inline bool IsProceduralMeshOutsideLightFrustum(const glm::mat4& InWorld, const FMeshComponent& InMesh,
                                                    const FFrustumPlanes& InFrustum) {
        glm::vec3 e = ProceduralMeshLocalExtent(InMesh);
        glm::vec3 wMin, wMax;
        TransformAABB(-e, e, InWorld, wMin, wMax);
        return !AABBIntersectsFrustum(wMin, wMax, InFrustum);
    }

    inline bool IsStaticMeshOutsideLightFrustum(const glm::mat4& InWorld, const FStaticMeshComponent& InMesh,
                                                const FFrustumPlanes& InFrustum) {
        if (!InMesh.StaticMesh)
            return false;
        glm::vec3 wMin, wMax;
        TransformAABB(InMesh.StaticMesh->GetBoundsMin(), InMesh.StaticMesh->GetBoundsMax(), InWorld, wMin, wMax);
        return !AABBIntersectsFrustum(wMin, wMax, InFrustum);
    }

    inline bool IsSkeletalMeshOutsideLightFrustum(const glm::mat4& InWorld, const FSkinnedMeshRenderState& InMesh,
                                                  const FFrustumPlanes& InFrustum) {
        if (!InMesh.SkeletalMesh)
            return false;
        glm::mat4 relative = glm::translate(glm::mat4(1.0f), InMesh.RelativeLocation) *
                             glm::toMat4(glm::quat(glm::radians(InMesh.RelativeRotation))) *
                             glm::scale(glm::mat4(1.0f), InMesh.RelativeScale);
        glm::vec3 wMin, wMax;
        // Bind-pose AABB: animation can leave these bounds, so inflate instead of per-bone AABBs.
        TransformAABB(InMesh.SkeletalMesh->GetBoundsMin(), InMesh.SkeletalMesh->GetBoundsMax(), InWorld * relative,
                      wMin, wMax);
        glm::vec3 center = 0.5f * (wMin + wMax);
        glm::vec3 extent = (wMax - wMin) * 0.5f * FShadowSettings::kSkinnedShadowBoundsPadding;
        wMin = center - extent;
        wMax = center + extent;
        return !AABBIntersectsFrustum(wMin, wMax, InFrustum);
    }

    inline float TransparentSortDistanceSq(const glm::mat4& InModel, const glm::vec3& InLocalMin,
                                           const glm::vec3& InLocalMax, const glm::vec3& InCamPos) {
        glm::vec3 wMin, wMax;
        TransformAABB(InLocalMin, InLocalMax, InModel, wMin, wMax);
        glm::vec3 center = 0.5f * (wMin + wMax);
        glm::vec3 delta = center - InCamPos;
        return glm::dot(delta, delta);
    }

    inline void ApplyShadowCasterRasterState(FMaterialInstance* InMat, bool bInCullFront) {
        bool bDoubleSided = InMat && InMat->GetDoubleSided();
        if (bDoubleSided) {
            FRenderCommand::SetCulling(false);
            return;
        }
        FRenderCommand::SetCulling(true, bInCullFront ? ECullMode::Front : ECullMode::Back);
    }

    inline void BindShadowCasterAlpha(FShader& InShader, FMaterialInstance* InMat) {
        if (!InMat || InMat->GetAlphaMode() != EAlphaMode::Mask) {
            InShader.SetInt("u_AlphaMode", 0);
            InShader.SetInt("u_UseAlbedoMap", 0);
            return;
        }
        TRef<FTexture2D> albedoTex = InMat->GetTexture(0);
        glm::vec2 tiling = InMat->GetUVTiling();
        glm::vec2 offset = InMat->GetUVOffset();
        InShader.SetInt("u_AlphaMode", 1);
        InShader.SetFloat("u_AlphaCutoff", InMat->GetAlphaCutoff());
        InShader.SetInt("u_UseAlbedoMap", albedoTex ? 1 : 0);
        InShader.SetFloat2("u_UVTiling", tiling.x, tiling.y);
        InShader.SetFloat2("u_UVOffset", offset.x, offset.y);
        if (albedoTex)
            albedoTex->Bind(0);
    }

    inline void BindLightmapUniforms(FShader& InShader, bool bUseLightmap, bool bUseTexCoord, const glm::vec2& InScale,
                              const glm::vec2& InBias, const TRef<FTexture2D>& InTexture) {
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

    // Planar capture is a static-world pass. Hidden or Movable meshes (pickups, projectiles)
    // must not contribute — otherwise a collected pickup leaves a ghost in the floor/mirror.
    inline bool CanContributeToPlanarReflection(bool bVisible, bool bVisibleInReflection,
                                                EComponentMobility InMobility) {
        return bVisible && bVisibleInReflection && InMobility != EComponentMobility::Movable;
    }

} // namespace Leon
