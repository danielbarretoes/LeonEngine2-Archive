#pragma once

#include "Assets/FAnimTypes.hpp"
#include "Engine/Components.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FRenderer.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"
#include "Renderer/FFrustumCull.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

namespace Leon {

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

    inline bool IsStaticMeshCulled(const FTransformComponent& InTransform, const FStaticMeshComponent& InMesh,
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

    inline bool IsSkeletalMeshCulled(const FTransformComponent& InTransform, const FSkinnedMeshRenderState& InMesh,
                              const FFrustumPlanes& InFrustum) {
        if (!InMesh.bVisible)
            return true;
        if (!InMesh.SkeletalMesh)
            return false;
        glm::mat4 model = InTransform.GetTransform();
        glm::mat4 relative = glm::translate(glm::mat4(1.0f), InMesh.RelativeLocation) *
                             glm::toMat4(glm::quat(glm::radians(InMesh.RelativeRotation))) *
                             glm::scale(glm::mat4(1.0f), InMesh.RelativeScale);
        glm::vec3 wMin, wMax;
        TransformAABB(InMesh.SkeletalMesh->GetBoundsMin(), InMesh.SkeletalMesh->GetBoundsMax(), model * relative,
                      wMin, wMax);
        if (!AABBIntersectsFrustum(wMin, wMax, InFrustum)) {
            FRenderer::GetStatsMutable().MeshesCulled++;
            return true;
        }
        FRenderer::GetStatsMutable().MeshesDrawn++;
        return false;
    }

    inline glm::mat4 SkeletalModelMatrix(const FTransformComponent& InTransform, const FSkinnedMeshRenderState& InMesh,
                                  const glm::mat4& InSubmeshLocal) {
        glm::mat4 relative = glm::translate(glm::mat4(1.0f), InMesh.RelativeLocation) *
                             glm::toMat4(glm::quat(glm::radians(InMesh.RelativeRotation))) *
                             glm::scale(glm::mat4(1.0f), InMesh.RelativeScale);
        return InTransform.GetTransform() * relative * InSubmeshLocal;
    }

    inline void UploadBonePalette(FUniformBuffer* InUBO, const std::vector<glm::mat4>& InPalette) {
        if (!InUBO)
            return;
        alignas(16) glm::mat4 padded[kMaxBones];
        for (uint32_t i = 0; i < kMaxBones; ++i)
            padded[i] = (i < InPalette.size()) ? InPalette[i] : glm::mat4(1.0f);
        InUBO->SetData(padded, sizeof(padded), 0);
    }

    inline bool IsProceduralMeshCulled(const FTransformComponent& InTransform, const FMeshComponent& InMesh,
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

} // namespace Leon
