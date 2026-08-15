#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <vector>
#include <cmath>

TEST_SUITE("Shader GPU - Post-Processing Tone Mapping & ACES Pipeline") {

    TEST_CASE("ACES Hardware Curve Numerical Accuracy & Strict Monotonicity") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/ToneMapping.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        GLuint hdrTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
        glTextureStorage2D(hdrTex, 1, GL_RGBA16F, 1, 1);
        glTextureParameteri(hdrTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(hdrTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTextureUnit(0, hdrTex);
        glBindTextureUnit(1, 0);
        shader->SetInt("u_HDRSceneTexture", 0);
        shader->SetInt("u_BloomTexture", 1);
        shader->SetInt("u_UseBloom", 0);
        shader->SetFloat("u_BloomIntensity", 0.0f);
        shader->SetInt("u_ToneMapper", 0); // 0 = ACES
        shader->SetFloat("u_Exposure", 1.0f);
        shader->SetFloat("u_Gamma", 2.2f);
        shader->SetInt("u_DebugMode", 0);

        // 1. Black Input (0.0) -> Output strictly 0.0
        float val0[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, val0);
        gl.DrawQuad();
        glm::vec4 pixel0 = gl.ReadPixel(0, 0);
        CHECK(pixel0.r == doctest::Approx(0.0f).epsilon(1e-5f));

        // 2. Middle Gray (0.18) -> Exact ACES value 0.266876 -> sRGB gamma = 0.54807
        float valMid[4] = { 0.18f, 0.18f, 0.18f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, valMid);
        gl.DrawQuad();
        glm::vec4 pixelMid = gl.ReadPixel(0, 0);
        CHECK(pixelMid.r == doctest::Approx(0.54807f).epsilon(0.015f));
        // Luma in alpha must equal grayscale channel value
        CHECK(pixelMid.a == doctest::Approx(pixelMid.r).epsilon(0.01f));

        // 3. Unit White (1.0) -> Exact ACES value 0.803797 -> sRGB gamma = 0.90561
        float val1[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, val1);
        gl.DrawQuad();
        glm::vec4 pixel1 = gl.ReadPixel(0, 0);
        CHECK(pixel1.r == doctest::Approx(0.90561f).epsilon(0.015f));

        // 4. Extreme HDR Brightness (1000.0) -> Bounded strictly <= 1.0
        float val1000[4] = { 1000.0f, 1000.0f, 1000.0f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, val1000);
        gl.DrawQuad();
        glm::vec4 pixel1000 = gl.ReadPixel(0, 0);
        CHECK(!std::isnan(pixel1000.r));
        CHECK(!std::isinf(pixel1000.r));
        CHECK(pixel1000.r <= 1.0001f);
        CHECK(pixel1000.r >= 0.999f);

        // 5. Strict Monotonicity: 0.0 < 0.18 < 1.0 < 1000.0
        CHECK(pixel0.r < pixelMid.r);
        CHECK(pixelMid.r < pixel1.r);
        CHECK(pixel1.r < pixel1000.r);

        glDeleteTextures(1, &hdrTex);
    }

    TEST_CASE("Tone Mapping Exposure Scaling & Multi-Operator Support") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping GPU shader test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/ToneMapping.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        GLuint hdrTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
        glTextureStorage2D(hdrTex, 1, GL_RGBA16F, 1, 1);
        glBindTextureUnit(0, hdrTex);
        shader->SetInt("u_HDRSceneTexture", 0);
        shader->SetInt("u_UseBloom", 0);
        shader->SetFloat("u_Gamma", 2.2f);
        shader->SetInt("u_DebugMode", 0);

        float val[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, val);

        // 1. Exposure 1.0 vs Exposure 2.0 (Double exposure must increase output)
        shader->SetInt("u_ToneMapper", 0);
        shader->SetFloat("u_Exposure", 1.0f);
        gl.DrawQuad();
        glm::vec4 pixelExp1 = gl.ReadPixel(0, 0);

        shader->SetFloat("u_Exposure", 2.0f);
        gl.DrawQuad();
        glm::vec4 pixelExp2 = gl.ReadPixel(0, 0);

        CHECK(pixelExp1.r > 0.0f);
        CHECK(pixelExp2.r > pixelExp1.r);

        // 2. Reinhard Operator (ToneMapper = 1)
        shader->SetInt("u_ToneMapper", 1);
        shader->SetFloat("u_Exposure", 1.0f);
        gl.DrawQuad();
        glm::vec4 pixelReinhard = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(pixelReinhard.r));
        CHECK(pixelReinhard.r > 0.0f);
        CHECK(pixelReinhard.r <= 1.0f);

        // 3. Neutral Clamped Operator (ToneMapper = 2) -> (0.5)^ (1/2.2) = 0.72974
        shader->SetInt("u_ToneMapper", 2);
        gl.DrawQuad();
        glm::vec4 pixelNeutral = gl.ReadPixel(0, 0);
        CHECK(pixelNeutral.r == doctest::Approx(0.72974f).epsilon(0.015f));

        glDeleteTextures(1, &hdrTex);
    }
}
