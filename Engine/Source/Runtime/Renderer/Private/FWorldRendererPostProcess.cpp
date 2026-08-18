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
    // Skybox Pass
    // =========================================================================
    void FWorldRenderer::RenderSkyboxPass(const FPerspectiveCamera& InCamera, const FSkyboxComponent* InSkybox,
                                          bool bHasDirLight, const FDirectionalLight& InDirLight) {
        if (!InSkybox || !InSkybox->bEnabled || !SkyboxShader || !SkyboxVA)
            return;

        // Cube is authored outward-CCW; the camera sits inside it, so back-face cull would drop every face.
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
        FRenderCommand::SetDepthMask(false);

        SkyboxShader->Bind();
        SkyboxShader->SetMat4("u_View", glm::value_ptr(InCamera.GetViewMatrix()));
        SkyboxShader->SetMat4("u_Projection", glm::value_ptr(InCamera.GetProjectionMatrix()));
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

    // =========================================================================
    // Post-Process Pass (Bloom + ACES Tonemapping + FXAA)
    // =========================================================================
    void FWorldRenderer::RenderPostProcessPass(float InExposure, uint32_t InTargetFBO, uint32_t InVpWidth,
                                               uint32_t InVpHeight, const FPerspectiveCamera& InCamera) {
        if (!HDRSceneFramebuffer)
            return;

        PostProcessSettings.Exposure = InExposure;
        FPostProcessFrameContext frame;
        frame.Projection = InCamera.GetProjectionMatrix();
        frame.InverseProjection = glm::inverse(frame.Projection);
        frame.bValid = true;
        PostProcessPipeline.Render(PostProcessSettings, HDRSceneFramebuffer, InTargetFBO, InVpWidth, InVpHeight,
                                   &frame);
    }

} // namespace Leon
