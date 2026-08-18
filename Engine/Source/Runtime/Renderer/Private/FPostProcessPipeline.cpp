#include "Renderer/FPostProcessPipeline.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include <random>

namespace Leon {

    FPostProcessPipeline::FPostProcessPipeline() = default;

    void FPostProcessPipeline::Init() {
        if (bInitialized)
            return;

        BloomBrightPassShader = FShader::Create("Engine/Assets/Shaders/BloomBrightPass.glsl");
        BloomDownsampleShader = FShader::Create("Engine/Assets/Shaders/BloomDownsample.glsl");
        BloomUpsampleShader = FShader::Create("Engine/Assets/Shaders/BloomUpsample.glsl");
        ToneMappingShader = FShader::Create("Engine/Assets/Shaders/ToneMapping.glsl");
        FXAAShader = FShader::Create("Engine/Assets/Shaders/FXAA.glsl");
        SSAOShader = FShader::Create("Engine/Assets/Shaders/SSAO.glsl");
        SSAOBlurShader = FShader::Create("Engine/Assets/Shaders/SSAOBlur.glsl");
        SSAOCompositeShader = FShader::Create("Engine/Assets/Shaders/SSAOComposite.glsl");

        std::mt19937 rng(0x5353414F);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        for (int i = 0; i < 16; ++i) {
            glm::vec3 sample(dist(rng), dist(rng), dist01(rng));
            sample = glm::normalize(sample);
            sample *= dist01(rng);
            float scale = static_cast<float>(i) / 16.0f;
            scale = 0.1f + scale * scale * 0.9f;
            SSAOKernel[i] = sample * scale;
        }

        FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);
        InvalidateFramebuffers(Width, Height);
        bInitialized = true;
    }

    void FPostProcessPipeline::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        if (InWidth == 0 || InHeight == 0)
            return;
        if (Width == InWidth && Height == InHeight && bInitialized)
            return;

        Width = InWidth;
        Height = InHeight;

        if (bInitialized) {
            InvalidateFramebuffers(Width, Height);
        }
    }

    void FPostProcessPipeline::InvalidateFramebuffers(uint32_t InWidth, uint32_t InHeight) {
        const uint32_t mipCount = 5;

        BloomDownsampleFBOs.resize(mipCount);
        BloomUpsampleFBOs.resize(mipCount);

        uint32_t currentWidth = std::max(InWidth / 2, 1u);
        uint32_t currentHeight = std::max(InHeight / 2, 1u);

        for (uint32_t i = 0; i < mipCount; ++i) {
            FFramebufferSpecification mipSpec;
            mipSpec.Width = currentWidth;
            mipSpec.Height = currentHeight;
            mipSpec.Attachments = {EFramebufferTextureFormat::RGBA16F};

            BloomDownsampleFBOs[i] = FFramebuffer::Create(mipSpec);
            BloomUpsampleFBOs[i] = FFramebuffer::Create(mipSpec);

            currentWidth = std::max(currentWidth / 2, 1u);
            currentHeight = std::max(currentHeight / 2, 1u);
        }

        // LDR Tone-Mapped Buffer (RGBA8)
        FFramebufferSpecification ldrSpec;
        ldrSpec.Width = InWidth;
        ldrSpec.Height = InHeight;
        ldrSpec.Attachments = {EFramebufferTextureFormat::RGBA8};
        ToneMappedFBO = FFramebuffer::Create(ldrSpec);

        const uint32_t halfW = std::max(InWidth / 2, 1u);
        const uint32_t halfH = std::max(InHeight / 2, 1u);
        FFramebufferSpecification ssaoSpec;
        ssaoSpec.Width = halfW;
        ssaoSpec.Height = halfH;
        ssaoSpec.Attachments = {EFramebufferTextureFormat::RGBA8};
        SSAOFBO = FFramebuffer::Create(ssaoSpec);
        SSAOBlurFBO = FFramebuffer::Create(ssaoSpec);

        FFramebufferSpecification ssaoColorSpec;
        ssaoColorSpec.Width = InWidth;
        ssaoColorSpec.Height = InHeight;
        ssaoColorSpec.Attachments = {EFramebufferTextureFormat::RGBA16F};
        SSAOCompositeFBO = FFramebuffer::Create(ssaoColorSpec);
    }

    void FPostProcessPipeline::RenderBloom(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                           uint32_t InWidth, uint32_t InHeight) {
        if (!InHDRScene || BloomDownsampleFBOs.empty())
            return;

        const uint32_t mipCount = std::min(static_cast<uint32_t>(BloomDownsampleFBOs.size()), InSettings.BloomMipCount);
        if (mipCount < 2)
            return;

        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);

        // ---------------------------------------------------------------------
        // Step 1: Bright-Pass Extraction (HDR Scene -> Mip 0 Downsample)
        // ---------------------------------------------------------------------
        auto& mip0FBO = BloomDownsampleFBOs[0];
        mip0FBO->Bind();
        FRenderCommand::SetViewport(0, 0, mip0FBO->GetSpecification().Width, mip0FBO->GetSpecification().Height);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();

        BloomBrightPassShader->Bind();
        InHDRScene->BindTexture(0, 0);
        BloomBrightPassShader->SetInt("u_HDRTexture", 0);
        BloomBrightPassShader->SetFloat("u_Threshold", InSettings.BloomThreshold);
        BloomBrightPassShader->SetFloat("u_SoftKnee", InSettings.BloomSoftKnee);

        FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(FullscreenQuadVA);
        mip0FBO->Unbind();

        // ---------------------------------------------------------------------
        // Step 2: Downsample Chain (13-tap Jimenez/Karis)
        // ---------------------------------------------------------------------
        BloomDownsampleShader->Bind();
        BloomDownsampleShader->SetInt("u_SourceTexture", 0);

        for (uint32_t i = 1; i < mipCount; ++i) {
            auto& destFBO = BloomDownsampleFBOs[i];
            auto& srcFBO = BloomDownsampleFBOs[i - 1];

            destFBO->Bind();
            FRenderCommand::SetViewport(0, 0, destFBO->GetSpecification().Width, destFBO->GetSpecification().Height);
            FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            FRenderCommand::Clear();

            srcFBO->BindTexture(0, 0);
            float texelX = 1.0f / static_cast<float>(srcFBO->GetSpecification().Width);
            float texelY = 1.0f / static_cast<float>(srcFBO->GetSpecification().Height);
            BloomDownsampleShader->SetFloat2("u_TexelSize", texelX, texelY);
            BloomDownsampleShader->SetInt("u_MipLevel", static_cast<int>(i - 1));

            FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(FullscreenQuadVA);
            destFBO->Unbind();
        }

        // ---------------------------------------------------------------------
        // Step 3: Upsample Chain (9-tap 3x3 Tent Filter with Additive Blend)
        // ---------------------------------------------------------------------
        BloomUpsampleShader->Bind();
        BloomUpsampleShader->SetInt("u_SourceTexture", 0);
        BloomUpsampleShader->SetFloat("u_FilterRadius", InSettings.BloomRadius);

        for (int i = static_cast<int>(mipCount) - 1; i > 0; --i) {
            auto& destUpsampleFBO = BloomUpsampleFBOs[i - 1];
            auto& srcDownsampleFBO = BloomDownsampleFBOs[i - 1];

            // Source for upsampling is either the lower upsample FBO or the lowest downsample FBO
            TRef<FFramebuffer> srcHigherMipFBO =
                (i == static_cast<int>(mipCount) - 1) ? BloomDownsampleFBOs[i] : BloomUpsampleFBOs[i];

            destUpsampleFBO->Bind();
            FRenderCommand::SetViewport(0, 0, destUpsampleFBO->GetSpecification().Width,
                                        destUpsampleFBO->GetSpecification().Height);
            FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            FRenderCommand::Clear();

            // Pass 3a: Base layer (copy downsampled content of current level)
            FRenderCommand::SetBlendState(false);
            BloomUpsampleShader->SetFloat("u_FilterRadius", 0.0f); // 0 radius = sample directly
            float downTexelX = 1.0f / static_cast<float>(srcDownsampleFBO->GetSpecification().Width);
            float downTexelY = 1.0f / static_cast<float>(srcDownsampleFBO->GetSpecification().Height);
            BloomUpsampleShader->SetFloat2("u_TexelSize", downTexelX, downTexelY);
            srcDownsampleFBO->BindTexture(0, 0);
            FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(FullscreenQuadVA);

            // Pass 3b: Additive tent upsample of higher blur mip
            FRenderCommand::SetBlendState(true);
            FRenderCommand::SetBlendFunc(EBlendFactor::One, EBlendFactor::One);

            BloomUpsampleShader->SetFloat("u_FilterRadius", InSettings.BloomRadius);
            float srcTexelX = 1.0f / static_cast<float>(srcHigherMipFBO->GetSpecification().Width);
            float srcTexelY = 1.0f / static_cast<float>(srcHigherMipFBO->GetSpecification().Height);
            BloomUpsampleShader->SetFloat2("u_TexelSize", srcTexelX, srcTexelY);
            srcHigherMipFBO->BindTexture(0, 0);
            FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(FullscreenQuadVA);

            FRenderCommand::SetBlendState(false);
            destUpsampleFBO->Unbind();
        }
    }

    TRef<FFramebuffer> FPostProcessPipeline::RenderSSAO(const FPostProcessSettings& InSettings,
                                                        TRef<FFramebuffer> InHDRScene,
                                                        const FPostProcessFrameContext& InFrame) {
        if (!InHDRScene || InHDRScene->GetDepthAttachmentRendererID() == 0 || !SSAOShader || !SSAOFBO || !SSAOBlurFBO ||
            !SSAOCompositeFBO || !SSAOCompositeShader || !FullscreenQuadVA)
            return InHDRScene;

        const uint32_t halfW = SSAOFBO->GetSpecification().Width;
        const uint32_t halfH = SSAOFBO->GetSpecification().Height;

        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);

        SSAOFBO->Bind();
        FRenderCommand::SetViewport(0, 0, halfW, halfH);
        FRenderCommand::SetClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        FRenderCommand::Clear();
        SSAOShader->Bind();
        InHDRScene->BindDepthTexture(0);
        SSAOShader->SetInt("u_DepthTexture", 0);
        SSAOShader->SetMat4("u_Projection", glm::value_ptr(InFrame.Projection));
        SSAOShader->SetMat4("u_InverseProjection", glm::value_ptr(InFrame.InverseProjection));
        SSAOShader->SetFloat("u_Radius", InSettings.SSAORadius);
        SSAOShader->SetFloat("u_Bias", InSettings.SSAOBias);
        int kernelSize = std::clamp(InSettings.SSAOKernelSize, 1, 16);
        SSAOShader->SetInt("u_KernelSize", kernelSize);
        for (int i = 0; i < 16; ++i)
            SSAOShader->SetFloat3("u_Samples[" + std::to_string(i) + "]", SSAOKernel[i].x, SSAOKernel[i].y,
                                  SSAOKernel[i].z);
        FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(FullscreenQuadVA);
        SSAOFBO->Unbind();

        auto blurPass = [&](TRef<FFramebuffer> InSrc, TRef<FFramebuffer> InDst, int InHorizontal) {
            InDst->Bind();
            FRenderCommand::SetViewport(0, 0, halfW, halfH);
            FRenderCommand::Clear();
            SSAOBlurShader->Bind();
            InSrc->BindTexture(0, 0);
            InHDRScene->BindDepthTexture(1);
            SSAOBlurShader->SetInt("u_SSAOTexture", 0);
            SSAOBlurShader->SetInt("u_DepthTexture", 1);
            SSAOBlurShader->SetFloat2("u_TexelSize", 1.0f / static_cast<float>(halfW),
                                      1.0f / static_cast<float>(halfH));
            SSAOBlurShader->SetInt("u_Horizontal", InHorizontal);
            FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(FullscreenQuadVA);
            InDst->Unbind();
        };
        blurPass(SSAOFBO, SSAOBlurFBO, 1);
        blurPass(SSAOBlurFBO, SSAOFBO, 0);

        SSAOCompositeFBO->Bind();
        FRenderCommand::SetViewport(0, 0, SSAOCompositeFBO->GetSpecification().Width,
                                    SSAOCompositeFBO->GetSpecification().Height);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();
        SSAOCompositeShader->Bind();
        InHDRScene->BindTexture(0, 0);
        SSAOFBO->BindTexture(0, 1);
        SSAOCompositeShader->SetInt("u_HDRSceneTexture", 0);
        SSAOCompositeShader->SetInt("u_SSAOTexture", 1);
        SSAOCompositeShader->SetFloat("u_Intensity", InSettings.SSAOIntensity);
        SSAOCompositeShader->SetInt("u_DebugAO", InSettings.DebugMode == 5 ? 1 : 0);
        FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(FullscreenQuadVA);
        SSAOCompositeFBO->Unbind();
        return SSAOCompositeFBO;
    }

    void FPostProcessPipeline::Render(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                      uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight,
                                      const FPostProcessFrameContext* InFrame) {
        if (!bInitialized) {
            Init();
        }

        if (!InHDRScene || !FullscreenQuadVA || !ToneMappedFBO)
            return;

        // Check if resize is required
        if (Width != InVpWidth || Height != InVpHeight) {
            OnViewportResize(InVpWidth, InVpHeight);
        }

        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);

        TRef<FFramebuffer> scene = InHDRScene;
        const bool bRunSSAO =
            InSettings.bEnabled && InSettings.bSSAOEnabled && InFrame && InFrame->bValid && InSettings.DebugMode != 1;
        if (bRunSSAO)
            scene = RenderSSAO(InSettings, InHDRScene, *InFrame);

        bool bRunBloom = InSettings.bEnabled && InSettings.bBloomEnabled && (InSettings.DebugMode != 1) &&
                         (InSettings.DebugMode != 5);
        if (bRunBloom) {
            RenderBloom(InSettings, scene, InVpWidth, InVpHeight);
        }

        // ---------------------------------------------------------------------
        // PASS 2: Tone Mapping & Composite (HDR Scene + Bloom -> LDR ToneMappedFBO)
        // ---------------------------------------------------------------------
        ToneMappedFBO->Bind();
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();

        ToneMappingShader->Bind();
        scene->BindTexture(0, 0);
        ToneMappingShader->SetInt("u_HDRSceneTexture", 0);

        if (bRunBloom && !BloomUpsampleFBOs.empty() && BloomUpsampleFBOs[0]) {
            BloomUpsampleFBOs[0]->BindTexture(0, 1);
        } else if (!BloomDownsampleFBOs.empty() && BloomDownsampleFBOs[0]) {
            BloomDownsampleFBOs[0]->BindTexture(0, 1);
        }
        ToneMappingShader->SetInt("u_BloomTexture", 1);

        ToneMappingShader->SetFloat("u_Exposure", InSettings.Exposure);
        ToneMappingShader->SetFloat("u_BloomIntensity", InSettings.BloomIntensity);
        ToneMappingShader->SetInt("u_UseBloom", bRunBloom ? 1 : 0);
        ToneMappingShader->SetInt("u_ToneMapper", InSettings.ToneMapper);
        ToneMappingShader->SetFloat("u_Gamma", InSettings.Gamma);
        ToneMappingShader->SetInt("u_DebugMode", InSettings.DebugMode);

        FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(FullscreenQuadVA);
        ToneMappedFBO->Unbind();

        // ---------------------------------------------------------------------
        // PASS 3: FXAA Anti-Aliasing (LDR ToneMappedFBO -> TargetFBO / Backbuffer)
        // ---------------------------------------------------------------------
        FRenderCommand::BindFramebuffer(InTargetFBO);
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);

        FXAAShader->Bind();
        ToneMappedFBO->BindTexture(0, 0);
        FXAAShader->SetInt("u_LDRTexture", 0);
        FXAAShader->SetFloat2("u_InverseScreenSize", 1.0f / static_cast<float>(InVpWidth),
                              1.0f / static_cast<float>(InVpHeight));
        FXAAShader->SetInt("u_FXAAEnabled",
                           (InSettings.bEnabled && InSettings.bFXAAEnabled && InSettings.DebugMode != 4) ? 1 : 0);

        FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(FullscreenQuadVA);

        // State Restoration
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
    }

} // namespace Leon
