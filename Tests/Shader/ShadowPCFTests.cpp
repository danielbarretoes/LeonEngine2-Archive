#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Shadow Filtering Modes (Hard, PCF 3x3, PCF 5x5, Poisson Disk)") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Multi-Filter Shadow Evaluation") {
        auto& gl = FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = FShader::Create("Engine/Resources/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.ResetShaderUniforms(shader);
        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        SetMat4(shader, "u_Model", glm::mat4(1.0f));
        SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));

        FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f); // Cascade 0
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.CascadeSplits = glm::vec4(5.0f, 15.0f, 35.0f, 100.0f);
        camData.ShadowParams = glm::vec4(0.001f, 0.002f, 0.0f, 0.0f); // Zero normal bias

        for (int c = 0; c < 4; ++c) {
            camData.LightSpaceMatrices[c] = glm::mat4(1.0f);
        }

        FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        shader->SetInt("u_UseShadows", 1);
        shader->SetInt("u_DebugMode", 24); // Mode 24: Direct Shadow Factor (1.0 = Lit, 0.0 = Occluded)

        SUBCASE("Hard Shadow Mode (1 Tap - Lit Invariant)") {
            camData.ShadowSettings = glm::ivec4(0, 16, 0, 24); // filterMode = 0 (Hard)
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Fully lit: 1.0 - shadow = 1.0
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("PCF 3x3 Mode (9 Taps - Filter Normalization)") {
            camData.ShadowSettings = glm::ivec4(1, 16, 0, 24); // filterMode = 1 (PCF3x3)
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // 9 taps of 1.0 / 9.0 = 1.0 -> Fully Lit (1.0)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("PCF 5x5 Mode (25 Taps - Filter Normalization)") {
            camData.ShadowSettings = glm::ivec4(2, 16, 0, 24); // filterMode = 2 (PCF5x5)
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // 25 taps of 1.0 / 25.0 = 1.0 -> Fully Lit (1.0)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("Poisson Disk Mode (16 Taps - Vogel Distribution Normalization)") {
            camData.ShadowSettings = glm::ivec4(3, 16, 0, 24); // filterMode = 3 (Poisson)
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // 16 taps of 1.0 / 16.0 = 1.0 -> Fully Lit (1.0)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("PCF 5x5 near UV 0 skips OOB taps (occluded, not border-lit)") {
            float zeroDepth = 0.0f;
            glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 0, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
                               &zeroDepth);

            glm::mat4 lightMat(1.0f);
            lightMat[3][0] = -0.90f;
            for (int c = 0; c < 4; ++c)
                camData.LightSpaceMatrices[c] = lightMat;
            camData.ShadowSettings = glm::ivec4(2, 16, 0, 24);
            camData.ShadowParams = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            float oneDepth = 1.0f;
            glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 0, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
                               &oneDepth);
            for (int c = 0; c < 4; ++c)
                camData.LightSpaceMatrices[c] = glm::mat4(1.0f);

            CHECK(pix.r == doctest::Approx(0.0f).epsilon(0.08f));
        }

        SUBCASE("Poisson Disk vs Hard Filter Penumbra Edge Response") {
            // Position quad right at shadow boundary in light space
            // deltaZ maps center to 1.000 (edge of shadow depth)
            glm::mat4 lightMat = glm::mat4(1.0f);
            lightMat[3][2] = 1.000f;
            for (int c = 0; c < 4; ++c)
                camData.LightSpaceMatrices[c] = lightMat;

            // In Poisson mode, sampling points with radius > 0 sample outside center
            camData.ShadowSettings = glm::ivec4(3, 16, 0, 24); // Poisson (mode 3)
            camData.ShadowParams = glm::vec4(0.0005f, 0.0f, 0.0f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pixPoisson = gl.ReadPixel(0, 0);
            CHECK(pixPoisson.r == doctest::Approx(1.0f).epsilon(0.02f));
        }
    }
}
