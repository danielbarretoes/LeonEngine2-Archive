#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FPostProcessPipeline.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include <vector>
#include <cmath>

TEST_SUITE("Shader GPU - End-to-End Post-Processing Pipeline") {

    TEST_CASE("End-to-End Pipeline Synthetic Scene (HDR -> Bloom -> Tone Mapping -> FXAA -> Output)") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        gl.BindDefaultTextures();

        const uint32_t width = 32;
        const uint32_t height = 32;

        Leon::FPostProcessPipeline pipeline;
        pipeline.Init();
        pipeline.OnViewportResize(width, height);

        // 1. Create a Synthetic HDR Scene FFramebuffer (32x32 RGBA16F)
        Leon::FFramebufferSpecification hdrSpec;
        hdrSpec.Width  = width;
        hdrSpec.Height = height;
        hdrSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA16F};
        auto hdrSceneFBO = Leon::FFramebuffer::Create(hdrSpec);
        REQUIRE(hdrSceneFBO != nullptr);

        // 2. Create Target Output FFramebuffer (32x32 RGBA8)
        Leon::FFramebufferSpecification outSpec;
        outSpec.Width  = width;
        outSpec.Height = height;
        outSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8};
        auto targetFBO = Leon::FFramebuffer::Create(outSpec);
        REQUIRE(targetFBO != nullptr);

        Leon::FPostProcessSettings settings;
        settings.bEnabled       = true;
        settings.bBloomEnabled  = true;
        settings.BloomThreshold = 1.0f;
        settings.BloomSoftKnee  = 0.5f;
        settings.BloomIntensity = 0.1f;
        settings.BloomRadius    = 1.0f;
        settings.ToneMapper     = 0; // ACES
        settings.Exposure       = 1.0f;
        settings.Gamma          = 2.2f;
        settings.bFXAAEnabled   = true;
        settings.DebugMode      = 0;

        // Case A: Pure Black Scene FInput -> Must produce strictly black output
        std::vector<float> blackPixels(width * height * 4, 0.0f);
        GLuint hdrTexID = hdrSceneFBO->GetColorAttachmentRendererID(0);
        glTextureSubImage2D(hdrTexID, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, blackPixels.data());

        pipeline.Render(settings, hdrSceneFBO, targetFBO->GetRendererID(), width, height);

        std::vector<uint8_t> outBlack(width * height * 4, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, targetFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outBlack.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        for (size_t i = 0; i < outBlack.size(); i += 4) {
            CHECK(outBlack[i + 0] == 0);
            CHECK(outBlack[i + 1] == 0);
            CHECK(outBlack[i + 2] == 0);
        }

        // Case B: Isolated Bright Hotspot (Center 4 pixels = 15.0 HDR, rest = 0.0)
        // Center should tonemap to white, and neighbor pixels should receive bloom glow!
        std::vector<float> hotspotPixels(width * height * 4, 0.0f);
        for (uint32_t y = 14; y <= 17; ++y) {
            for (uint32_t x = 14; x <= 17; ++x) {
                size_t idx = (y * width + x) * 4;
                hotspotPixels[idx + 0] = 15.0f;
                hotspotPixels[idx + 1] = 15.0f;
                hotspotPixels[idx + 2] = 15.0f;
                hotspotPixels[idx + 3] = 1.0f;
            }
        }
        glTextureSubImage2D(hdrTexID, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, hotspotPixels.data());

        pipeline.Render(settings, hdrSceneFBO, targetFBO->GetRendererID(), width, height);

        std::vector<uint8_t> outHotspot(width * height * 4, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, targetFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outHotspot.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Center pixel must be brightly saturated LDR
        size_t centerIdx = (15 * width + 15) * 4;
        CHECK(outHotspot[centerIdx + 0] > 200);
        CHECK(outHotspot[centerIdx + 1] > 200);
        CHECK(outHotspot[centerIdx + 2] > 200);

        // Immediate neighbor (outside original 4x4 hotspot) must have received positive bloom radiance!
        size_t neighborIdx = (12 * width + 12) * 4;
        CHECK(outHotspot[neighborIdx + 0] > 0);
        CHECK(outHotspot[neighborIdx + 1] > 0);
        CHECK(outHotspot[neighborIdx + 2] > 0);
    }

    TEST_CASE("Pipeline Dynamic Viewport Resize & FFramebuffer Lifetime Invariants") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        Leon::FPostProcessPipeline pipeline;
        pipeline.Init();

        // 1. Initial 1280x720
        pipeline.OnViewportResize(1280, 720);
        CHECK(pipeline.GetToneMappedFBO() != nullptr);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Width == 1280);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Height == 720);

        // Check Bloom pyramid mips
        const auto& downMips = pipeline.GetBloomDownsampleFBOs();
        REQUIRE(downMips.size() == 5);
        CHECK(downMips[0]->GetSpecification().Width == 640);
        CHECK(downMips[0]->GetSpecification().Height == 360);
        CHECK(downMips[4]->GetSpecification().Width == 40);
        CHECK(downMips[4]->GetSpecification().Height == 22);

        // 2. Resize to 1920x1080 (Full HD)
        pipeline.OnViewportResize(1920, 1080);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Width == 1920);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Height == 1080);
        CHECK(downMips[0]->GetSpecification().Width == 960);
        CHECK(downMips[0]->GetSpecification().Height == 540);

        // 3. Resize back to 1280x720
        pipeline.OnViewportResize(1280, 720);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Width == 1280);
        CHECK(pipeline.GetToneMappedFBO()->GetSpecification().Height == 720);
        CHECK(downMips[0]->GetSpecification().Width == 640);
        CHECK(downMips[0]->GetSpecification().Height == 360);

        // Verify zero OpenGL errors occurred during reallocation
        GLenum err = glGetError();
        CHECK(err == GL_NO_ERROR);
    }
}
