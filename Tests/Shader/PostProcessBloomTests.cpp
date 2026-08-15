#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "renderer/PostProcessPipeline.hpp"
#include <vector>
#include <cmath>

TEST_SUITE("Shader GPU - Post-Processing Bloom Pipeline") {

    TEST_CASE("Bloom Bright-Pass GPU Thresholding & Soft-Knee Invariants") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/BloomBrightPass.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        // 1. Create a 1x1 HDR Input Texture
        GLuint hdrTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
        glTextureStorage2D(hdrTex, 1, GL_RGBA16F, 1, 1);
        glBindTextureUnit(0, hdrTex);
        shader->SetInt("u_HDRTexture", 0);

        shader->SetFloat("u_Threshold", 1.0f);
        shader->SetFloat("u_SoftKnee", 0.5f);

        // Case A: Below Threshold and below knee (L = 0.2 < 0.5) -> Output must be strictly 0.0
        float valBelow[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, valBelow);
        gl.DrawQuad();
        glm::vec4 pixelBelow = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(pixelBelow.r));
        CHECK(!std::isinf(pixelBelow.r));
        CHECK(pixelBelow.r == doctest::Approx(0.0f).epsilon(1e-4f));
        CHECK(pixelBelow.g == doctest::Approx(0.0f).epsilon(1e-4f));
        CHECK(pixelBelow.b == doctest::Approx(0.0f).epsilon(1e-4f));

        // Case B: In Soft-Knee Transition (L = 0.9, in [0.5, 1.5]) -> Exact quadratic knee output = 0.08
        float valMid[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, valMid);
        gl.DrawQuad();
        glm::vec4 pixelMid = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(pixelMid.r));
        CHECK(pixelMid.r == doctest::Approx(0.0800f).epsilon(0.005f));
        CHECK(pixelMid.g == doctest::Approx(0.0800f).epsilon(0.005f));
        CHECK(pixelMid.b == doctest::Approx(0.0800f).epsilon(0.005f));

        // Case C: High Luminance HDR Value (L = 5.0) -> Output strongly positive
        float valHigh[4] = { 5.0f, 5.0f, 5.0f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, valHigh);
        gl.DrawQuad();
        glm::vec4 pixelHigh = gl.ReadPixel(0, 0);

        // For L = 5.0, Threshold = 1.0, contribution = (5 - 1)/5 = 0.80 -> output = 5.0 * 0.80 = 4.0
        CHECK(pixelHigh.r == doctest::Approx(4.0f).epsilon(0.02f));
        CHECK(pixelHigh.g == doctest::Approx(4.0f).epsilon(0.02f));
        CHECK(pixelHigh.b == doctest::Approx(4.0f).epsilon(0.02f));

        // Case D: Pure Black (L = 0.0) -> Output strictly 0.0
        float valBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, valBlack);
        gl.DrawQuad();
        glm::vec4 pixelBlack = gl.ReadPixel(0, 0);

        CHECK(pixelBlack.r == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelBlack.g == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelBlack.b == doctest::Approx(0.0f).epsilon(1e-5f));

        glDeleteTextures(1, &hdrTex);
    }

    TEST_CASE("Bloom Downsampling & Upsampling Filter Mathematical Energy Bounds") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto downShader = Leon::FShader::Create("Engine/Assets/Shaders/BloomDownsample.glsl");
        auto upShader   = Leon::FShader::Create("Engine/Assets/Shaders/BloomUpsample.glsl");
        REQUIRE(downShader != nullptr);
        REQUIRE(upShader != nullptr);

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        // 4x4 Constant Input Texture of value 2.0f
        GLuint srcTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &srcTex);
        glTextureStorage2D(srcTex, 1, GL_RGBA16F, 4, 4);
        std::vector<float> constData(4 * 4 * 4, 2.0f);
        glTextureSubImage2D(srcTex, 0, 0, 0, 4, 4, GL_RGBA, GL_FLOAT, constData.data());

        glBindTextureUnit(0, srcTex);

        // Downsample constant input -> must preserve average value ~2.0
        downShader->Bind();
        downShader->SetInt("u_SourceTexture", 0);
        downShader->SetFloat2("u_TexelSize", 1.0f / 4.0f, 1.0f / 4.0f);
        downShader->SetInt("u_MipLevel", 1); // linear weights

        gl.DrawQuad();
        glm::vec4 downPixel = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(downPixel.r));
        CHECK(downPixel.r == doctest::Approx(2.0f).epsilon(0.02f));

        // Upsample constant input -> tent filter preserves energy (sum of weights = 1.0)
        upShader->Bind();
        upShader->SetInt("u_SourceTexture", 0);
        upShader->SetFloat("u_FilterRadius", 1.0f);
        upShader->SetFloat2("u_TexelSize", 1.0f / 4.0f, 1.0f / 4.0f);

        gl.DrawQuad();
        glm::vec4 upPixel = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(upPixel.r));
        CHECK(upPixel.r == doctest::Approx(2.0f).epsilon(0.02f));

        glDeleteTextures(1, &srcTex);
    }
}
