#include "Renderer/FPostProcessPipeline.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include <algorithm>

namespace Leon {

    FPostProcessPipeline::FPostProcessPipeline() = default;

    void FPostProcessPipeline::Init() {
        if (bInitialized)
            return;

        // 1. Shaders
        BloomBrightPassShader = FShader::Create("Engine/Assets/Shaders/BloomBrightPass.glsl");
        BloomDownsampleShader = FShader::Create("Engine/Assets/Shaders/BloomDownsample.glsl");
        BloomUpsampleShader = FShader::Create("Engine/Assets/Shaders/BloomUpsample.glsl");
        ToneMappingShader = FShader::Create("Engine/Assets/Shaders/ToneMapping.glsl");
        FXAAShader = FShader::Create("Engine/Assets/Shaders/FXAA.glsl");

        // 2. Fullscreen Quad Mesh
        FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);

        // 3. Allocate Framebuffers
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
    }

    void FPostProcessPipeline::RenderBloom(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                           uint32_t InWidth, uint32_t InHeight) {
        if (!InHDRScene || BloomDownsampleFBOs.empty())
            return;

        const uint32_t mipCount =
            std::min(static_cast<uint32_t>(BloomDownsampleFBOs.size()), InSettings.BloomMipCount);
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

    void FPostProcessPipeline::Render(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                      uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight) {
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

        // ---------------------------------------------------------------------
        // PASS 1: Bloom Extraction and Pyramid (if enabled)
        // ---------------------------------------------------------------------
        bool bRunBloom = InSettings.bEnabled && InSettings.bBloomEnabled && (InSettings.DebugMode != 1);
        if (bRunBloom) {
            RenderBloom(InSettings, InHDRScene, InVpWidth, InVpHeight);
        }

        // ---------------------------------------------------------------------
        // PASS 2: Tone Mapping & Composite (HDR Scene + Bloom -> LDR ToneMappedFBO)
        // ---------------------------------------------------------------------
        ToneMappedFBO->Bind();
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();

        ToneMappingShader->Bind();
        InHDRScene->BindTexture(0, 0);
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
    }

} // namespace Leon
