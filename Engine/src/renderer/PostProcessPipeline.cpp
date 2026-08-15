#include "renderer/PostProcessPipeline.hpp"
#include "renderer/MeshPrimitives.hpp"
#include "renderer/RenderCommand.hpp"
#include <algorithm>

namespace Leon {

    FPostProcessPipeline::FPostProcessPipeline() = default;

    void FPostProcessPipeline::Init() {
        if (m_bInitialized)
            return;

        // 1. Shaders
        m_BloomBrightPassShader = FShader::Create("Engine/Assets/Shaders/BloomBrightPass.glsl");
        m_BloomDownsampleShader = FShader::Create("Engine/Assets/Shaders/BloomDownsample.glsl");
        m_BloomUpsampleShader = FShader::Create("Engine/Assets/Shaders/BloomUpsample.glsl");
        m_ToneMappingShader = FShader::Create("Engine/Assets/Shaders/ToneMapping.glsl");
        m_FXAAShader = FShader::Create("Engine/Assets/Shaders/FXAA.glsl");

        // 2. Fullscreen Quad Mesh
        m_FullscreenQuadVA = FMeshPrimitives::CreateQuad(2.0f, 2.0f);

        // 3. Allocate Framebuffers
        InvalidateFramebuffers(m_Width, m_Height);

        m_bInitialized = true;
    }

    void FPostProcessPipeline::OnViewportResize(uint32_t InWidth, uint32_t InHeight) {
        if (InWidth == 0 || InHeight == 0)
            return;
        if (m_Width == InWidth && m_Height == InHeight && m_bInitialized)
            return;

        m_Width = InWidth;
        m_Height = InHeight;

        if (m_bInitialized) {
            InvalidateFramebuffers(m_Width, m_Height);
        }
    }

    void FPostProcessPipeline::InvalidateFramebuffers(uint32_t InWidth, uint32_t InHeight) {
        const uint32_t mipCount = 5;

        m_BloomDownsampleFBOs.resize(mipCount);
        m_BloomUpsampleFBOs.resize(mipCount);

        uint32_t currentWidth = std::max(InWidth / 2, 1u);
        uint32_t currentHeight = std::max(InHeight / 2, 1u);

        for (uint32_t i = 0; i < mipCount; ++i) {
            FFramebufferSpecification mipSpec;
            mipSpec.Width = currentWidth;
            mipSpec.Height = currentHeight;
            mipSpec.Attachments = {EFramebufferTextureFormat::RGBA16F};

            m_BloomDownsampleFBOs[i] = FFramebuffer::Create(mipSpec);
            m_BloomUpsampleFBOs[i] = FFramebuffer::Create(mipSpec);

            currentWidth = std::max(currentWidth / 2, 1u);
            currentHeight = std::max(currentHeight / 2, 1u);
        }

        // LDR Tone-Mapped Buffer (RGBA8)
        FFramebufferSpecification ldrSpec;
        ldrSpec.Width = InWidth;
        ldrSpec.Height = InHeight;
        ldrSpec.Attachments = {EFramebufferTextureFormat::RGBA8};
        m_ToneMappedFBO = FFramebuffer::Create(ldrSpec);
    }

    void FPostProcessPipeline::RenderBloom(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                           uint32_t InWidth, uint32_t InHeight) {
        if (!InHDRScene || m_BloomDownsampleFBOs.empty())
            return;

        const uint32_t mipCount =
            std::min(static_cast<uint32_t>(m_BloomDownsampleFBOs.size()), InSettings.BloomMipCount);
        if (mipCount < 2)
            return;

        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(false);

        // ---------------------------------------------------------------------
        // Step 1: Bright-Pass Extraction (HDR Scene -> Mip 0 Downsample)
        // ---------------------------------------------------------------------
        auto& mip0FBO = m_BloomDownsampleFBOs[0];
        mip0FBO->Bind();
        FRenderCommand::SetViewport(0, 0, mip0FBO->GetSpecification().Width, mip0FBO->GetSpecification().Height);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();

        m_BloomBrightPassShader->Bind();
        InHDRScene->BindTexture(0, 0);
        m_BloomBrightPassShader->SetInt("u_HDRTexture", 0);
        m_BloomBrightPassShader->SetFloat("u_Threshold", InSettings.BloomThreshold);
        m_BloomBrightPassShader->SetFloat("u_SoftKnee", InSettings.BloomSoftKnee);

        m_FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(m_FullscreenQuadVA);
        mip0FBO->Unbind();

        // ---------------------------------------------------------------------
        // Step 2: Downsample Chain (13-tap Jimenez/Karis)
        // ---------------------------------------------------------------------
        m_BloomDownsampleShader->Bind();
        m_BloomDownsampleShader->SetInt("u_SourceTexture", 0);

        for (uint32_t i = 1; i < mipCount; ++i) {
            auto& destFBO = m_BloomDownsampleFBOs[i];
            auto& srcFBO = m_BloomDownsampleFBOs[i - 1];

            destFBO->Bind();
            FRenderCommand::SetViewport(0, 0, destFBO->GetSpecification().Width, destFBO->GetSpecification().Height);
            FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            FRenderCommand::Clear();

            srcFBO->BindTexture(0, 0);
            float texelX = 1.0f / static_cast<float>(srcFBO->GetSpecification().Width);
            float texelY = 1.0f / static_cast<float>(srcFBO->GetSpecification().Height);
            m_BloomDownsampleShader->SetFloat2("u_TexelSize", texelX, texelY);
            m_BloomDownsampleShader->SetInt("u_MipLevel", static_cast<int>(i - 1));

            m_FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(m_FullscreenQuadVA);
            destFBO->Unbind();
        }

        // ---------------------------------------------------------------------
        // Step 3: Upsample Chain (9-tap 3x3 Tent Filter with Additive Blend)
        // ---------------------------------------------------------------------
        m_BloomUpsampleShader->Bind();
        m_BloomUpsampleShader->SetInt("u_SourceTexture", 0);
        m_BloomUpsampleShader->SetFloat("u_FilterRadius", InSettings.BloomRadius);

        for (int i = static_cast<int>(mipCount) - 1; i > 0; --i) {
            auto& destUpsampleFBO = m_BloomUpsampleFBOs[i - 1];
            auto& srcDownsampleFBO = m_BloomDownsampleFBOs[i - 1];

            // Source for upsampling is either the lower upsample FBO or the lowest downsample FBO
            TRef<FFramebuffer> srcHigherMipFBO =
                (i == static_cast<int>(mipCount) - 1) ? m_BloomDownsampleFBOs[i] : m_BloomUpsampleFBOs[i];

            destUpsampleFBO->Bind();
            FRenderCommand::SetViewport(0, 0, destUpsampleFBO->GetSpecification().Width,
                                        destUpsampleFBO->GetSpecification().Height);
            FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            FRenderCommand::Clear();

            // Pass 3a: Base layer (copy downsampled content of current level)
            FRenderCommand::SetBlendState(false);
            m_BloomUpsampleShader->SetFloat("u_FilterRadius", 0.0f); // 0 radius = sample directly
            float downTexelX = 1.0f / static_cast<float>(srcDownsampleFBO->GetSpecification().Width);
            float downTexelY = 1.0f / static_cast<float>(srcDownsampleFBO->GetSpecification().Height);
            m_BloomUpsampleShader->SetFloat2("u_TexelSize", downTexelX, downTexelY);
            srcDownsampleFBO->BindTexture(0, 0);
            m_FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(m_FullscreenQuadVA);

            // Pass 3b: Additive tent upsample of higher blur mip
            FRenderCommand::SetBlendState(true);
            FRenderCommand::SetBlendFunc(EBlendFactor::One, EBlendFactor::One);

            m_BloomUpsampleShader->SetFloat("u_FilterRadius", InSettings.BloomRadius);
            float srcTexelX = 1.0f / static_cast<float>(srcHigherMipFBO->GetSpecification().Width);
            float srcTexelY = 1.0f / static_cast<float>(srcHigherMipFBO->GetSpecification().Height);
            m_BloomUpsampleShader->SetFloat2("u_TexelSize", srcTexelX, srcTexelY);
            srcHigherMipFBO->BindTexture(0, 0);
            m_FullscreenQuadVA->Bind();
            FRenderCommand::DrawIndexed(m_FullscreenQuadVA);

            FRenderCommand::SetBlendState(false);
            destUpsampleFBO->Unbind();
        }
    }

    void FPostProcessPipeline::Render(const FPostProcessSettings& InSettings, TRef<FFramebuffer> InHDRScene,
                                      uint32_t InTargetFBO, uint32_t InVpWidth, uint32_t InVpHeight) {
        if (!m_bInitialized) {
            Init();
        }

        if (!InHDRScene || !m_FullscreenQuadVA || !m_ToneMappedFBO)
            return;

        // Check if resize is required
        if (m_Width != InVpWidth || m_Height != InVpHeight) {
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
        m_ToneMappedFBO->Bind();
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);
        FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        FRenderCommand::Clear();

        m_ToneMappingShader->Bind();
        InHDRScene->BindTexture(0, 0);
        m_ToneMappingShader->SetInt("u_HDRSceneTexture", 0);

        if (bRunBloom && !m_BloomUpsampleFBOs.empty() && m_BloomUpsampleFBOs[0]) {
            m_BloomUpsampleFBOs[0]->BindTexture(0, 1);
        } else if (!m_BloomDownsampleFBOs.empty() && m_BloomDownsampleFBOs[0]) {
            m_BloomDownsampleFBOs[0]->BindTexture(0, 1);
        }
        m_ToneMappingShader->SetInt("u_BloomTexture", 1);

        m_ToneMappingShader->SetFloat("u_Exposure", InSettings.Exposure);
        m_ToneMappingShader->SetFloat("u_BloomIntensity", InSettings.BloomIntensity);
        m_ToneMappingShader->SetInt("u_UseBloom", bRunBloom ? 1 : 0);
        m_ToneMappingShader->SetInt("u_ToneMapper", InSettings.ToneMapper);
        m_ToneMappingShader->SetFloat("u_Gamma", InSettings.Gamma);
        m_ToneMappingShader->SetInt("u_DebugMode", InSettings.DebugMode);

        m_FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(m_FullscreenQuadVA);
        m_ToneMappedFBO->Unbind();

        // ---------------------------------------------------------------------
        // PASS 3: FXAA Anti-Aliasing (LDR ToneMappedFBO -> TargetFBO / Backbuffer)
        // ---------------------------------------------------------------------
        FRenderCommand::BindFramebuffer(InTargetFBO);
        FRenderCommand::SetViewport(0, 0, InVpWidth, InVpHeight);

        m_FXAAShader->Bind();
        m_ToneMappedFBO->BindTexture(0, 0);
        m_FXAAShader->SetInt("u_LDRTexture", 0);
        m_FXAAShader->SetFloat2("u_InverseScreenSize", 1.0f / static_cast<float>(InVpWidth),
                                1.0f / static_cast<float>(InVpHeight));
        m_FXAAShader->SetInt("u_FXAAEnabled",
                             (InSettings.bEnabled && InSettings.bFXAAEnabled && InSettings.DebugMode != 4) ? 1 : 0);

        m_FullscreenQuadVA->Bind();
        FRenderCommand::DrawIndexed(m_FullscreenQuadVA);

        // State Restoration
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
    }

} // namespace Leon
