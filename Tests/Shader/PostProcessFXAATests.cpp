#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <vector>
#include <cmath>

TEST_SUITE("Shader GPU - Post-Processing FXAA Pipeline") {

    TEST_CASE("FXAA Uniform Color Field Zero Distortion Invariant") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/FXAA.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        // 16x16 Output FBO
        Leon::FFramebufferSpecification fboSpec;
        fboSpec.Width = 16;
        fboSpec.Height = 16;
        fboSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8};
        auto testFBO = Leon::FFramebuffer::Create(fboSpec);
        REQUIRE(testFBO != nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, testFBO->GetRendererID());
        glViewport(0, 0, 16, 16);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 16x16 Uniform Color Texture (R=0.6, G=0.6, B=0.6, A=0.6)
        GLuint ldrTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &ldrTex);
        glTextureStorage2D(ldrTex, 1, GL_RGBA8, 16, 16);
        std::vector<uint8_t> uniformData(16 * 16 * 4, static_cast<uint8_t>(0.6f * 255.0f));
        glTextureSubImage2D(ldrTex, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, uniformData.data());

        glBindTextureUnit(0, ldrTex);
        shader->SetInt("u_LDRTexture", 0);
        shader->SetFloat2("u_InverseScreenSize", 1.0f / 16.0f, 1.0f / 16.0f);
        shader->SetInt("u_FXAAEnabled", 1);

        gl.DrawQuad();

        std::vector<uint8_t> readback(16 * 16 * 4, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, testFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());

        // Check across multiple sample coordinates (center, corners)
        for (int y = 0; y < 16; y += 4) {
            for (int x = 0; x < 16; x += 4) {
                size_t idx = (y * 16 + x) * 4;
                float r = static_cast<float>(readback[idx + 0]) / 255.0f;
                float g = static_cast<float>(readback[idx + 1]) / 255.0f;
                float b = static_cast<float>(readback[idx + 2]) / 255.0f;

                CHECK(!std::isnan(r));
                CHECK(!std::isinf(r));
                CHECK(r == doctest::Approx(0.6f).epsilon(0.02f));
                CHECK(g == doctest::Approx(0.6f).epsilon(0.02f));
                CHECK(b == doctest::Approx(0.6f).epsilon(0.02f));
            }
        }

        // Sub-threshold contrast test (100 on left, 108 on right -> below lumaMax * 0.125 = 0.053)
        std::vector<uint8_t> subThresholdData(16 * 16 * 4, 0);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                uint8_t val = (x >= 8) ? 108 : 100;
                size_t idx = (y * 16 + x) * 4;
                subThresholdData[idx + 0] = val;
                subThresholdData[idx + 1] = val;
                subThresholdData[idx + 2] = val;
                subThresholdData[idx + 3] = val;
            }
        }
        glTextureSubImage2D(ldrTex, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, subThresholdData.data());
        gl.DrawQuad();

        std::vector<uint8_t> readSub(16 * 16 * 4, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, testFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, readSub.data());
        CHECK(readSub[(8 * 16 + 7) * 4] == 100); // Must not blur sub-threshold differences
        CHECK(readSub[(8 * 16 + 8) * 4] == 108);

        glDeleteTextures(1, &ldrTex);
    }

    TEST_CASE("FXAA High-Contrast Edge Anti-Aliasing Response") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/FXAA.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();

        Leon::FFramebufferSpecification fboSpec;
        fboSpec.Width = 16;
        fboSpec.Height = 16;
        fboSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8};
        auto testFBO = Leon::FFramebuffer::Create(fboSpec);
        REQUIRE(testFBO != nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, testFBO->GetRendererID());
        glViewport(0, 0, 16, 16);

        // 16x16 Texture with horizontal bar (row 8 = 255, other rows = 0)
        GLuint edgeTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &edgeTex);
        glTextureStorage2D(edgeTex, 1, GL_RGBA8, 16, 16);

        std::vector<uint8_t> edgeData(16 * 16 * 4, 0);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                uint8_t val = (y == 8) ? 255 : 0;
                size_t idx = (y * 16 + x) * 4;
                edgeData[idx + 0] = val;
                edgeData[idx + 1] = val;
                edgeData[idx + 2] = val;
                edgeData[idx + 3] = val; // Luma in alpha
            }
        }
        glTextureSubImage2D(edgeTex, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, edgeData.data());

        glBindTextureUnit(0, edgeTex);
        shader->SetInt("u_LDRTexture", 0);
        shader->SetFloat2("u_InverseScreenSize", 1.0f / 16.0f, 1.0f / 16.0f);

        // 1. FXAA Disabled -> Hard edge preserved exactly
        shader->SetInt("u_FXAAEnabled", 0);
        gl.DrawQuad();

        std::vector<uint8_t> readbackOff(16 * 16 * 4, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, testFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, readbackOff.data());

        CHECK(readbackOff[(8 * 16 + 8) * 4] == 255);
        CHECK(readbackOff[(7 * 16 + 8) * 4] == 0);

        // 2. FXAA Enabled -> Directional horizontal edge is detected and anti-aliased vertically
        shader->SetInt("u_FXAAEnabled", 1);
        gl.DrawQuad();

        std::vector<uint8_t> readbackOn(16 * 16 * 4, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, readbackOn.data());

        float centerVal = static_cast<float>(readbackOn[(8 * 16 + 8) * 4]) / 255.0f;
        float neighborVal = static_cast<float>(readbackOn[(7 * 16 + 8) * 4]) / 255.0f;

        CHECK(!std::isnan(centerVal));
        CHECK(!std::isnan(neighborVal));
        CHECK(centerVal < 0.95f);   // Center peak intensity softened
        CHECK(neighborVal > 0.0f);  // Radiance distributed to orthogonal neighbor across the edge

        glDeleteTextures(1, &edgeTex);
    }

    TEST_CASE("FXAA Subpixel Anti-Aliasing on Isolated High-Frequency Feature") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/FXAA.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();

        Leon::FFramebufferSpecification fboSpec;
        fboSpec.Width = 16;
        fboSpec.Height = 16;
        fboSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8};
        auto testFBO = Leon::FFramebuffer::Create(fboSpec);
        REQUIRE(testFBO != nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, testFBO->GetRendererID());
        glViewport(0, 0, 16, 16);

        // 16x16 Texture: Black field with an isolated single white pixel at (8, 8)
        GLuint pointTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &pointTex);
        glTextureStorage2D(pointTex, 1, GL_RGBA8, 16, 16);

        std::vector<uint8_t> pointData(16 * 16 * 4, 0);
        size_t centerIdx = (8 * 16 + 8) * 4;
        pointData[centerIdx + 0] = 255;
        pointData[centerIdx + 1] = 255;
        pointData[centerIdx + 2] = 255;
        pointData[centerIdx + 3] = 255; // Luma in alpha

        glTextureSubImage2D(pointTex, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pointData.data());

        glBindTextureUnit(0, pointTex);
        shader->SetInt("u_LDRTexture", 0);
        shader->SetFloat2("u_InverseScreenSize", 1.0f / 16.0f, 1.0f / 16.0f);

        // When FXAA is ON: Subpixel blend must blend the isolated single-pixel hotspot
        shader->SetInt("u_FXAAEnabled", 1);
        gl.DrawQuad();

        std::vector<uint8_t> readback(16 * 16 * 4, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, testFBO->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());

        float centerVal = static_cast<float>(readback[centerIdx + 0]) / 255.0f;
        CHECK(!std::isnan(centerVal));
        CHECK(centerVal < 0.95f); // Peak intensity attenuated by subpixel filter
        CHECK(centerVal > 0.10f);

        glDeleteTextures(1, &pointTex);
    }
}
