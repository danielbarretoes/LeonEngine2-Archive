#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Shadow Cascade Slice Selection & False-Color Debug") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Cascade Slice Selection by Depth") {
        auto& gl = FHeadlessGLContext::Get();
        if (!gl.IsValid()) return;

        auto shader = FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.ResetShaderUniforms(shader);
        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        SetMat4(shader, "u_Model", glm::mat4(1.0f));
        SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));

        shader->SetInt("u_UseShadows", 1);
        shader->SetInt("u_DebugMode", 25); // Mode 25: Cascade Index False-Color

        FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.CascadeSplits  = glm::vec4(5.0f, 15.0f, 35.0f, 100.0f);
        camData.ShadowParams   = glm::vec4(0.001f, 0.002f, 0.02f, 0.0f); // No blend
        camData.ShadowSettings = glm::ivec4(1, 16, 0, 25);

        for (int c = 0; c < 4; ++c) camData.LightSpaceMatrices[c] = glm::mat4(1.0f);

        FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        SUBCASE("Cascade 0 Selection (Depth < 5.0 -> Red)") {
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f); // depth = 2.0
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Cascade 0 is Red (1.0, 0.15, 0.15)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.05f));
            CHECK(pix.g == doctest::Approx(0.15f).epsilon(0.05f));
            CHECK(pix.b == doctest::Approx(0.15f).epsilon(0.05f));
        }

        SUBCASE("Cascade 1 Selection (5.0 <= Depth < 15.0 -> Green)") {
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 10.0f, 1.0f); // depth = 10.0
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Cascade 1 is Green (0.15, 0.90, 0.20)
            CHECK(pix.r == doctest::Approx(0.15f).epsilon(0.05f));
            CHECK(pix.g == doctest::Approx(0.9f).epsilon(0.05f));
            CHECK(pix.b == doctest::Approx(0.20f).epsilon(0.05f));
        }

        SUBCASE("Cascade 2 Selection (15.0 <= Depth < 35.0 -> Blue)") {
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 25.0f, 1.0f); // depth = 25.0
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Cascade 2 is Blue (0.20, 0.40, 1.00)
            CHECK(pix.r == doctest::Approx(0.20f).epsilon(0.05f));
            CHECK(pix.g == doctest::Approx(0.40f).epsilon(0.05f));
            CHECK(pix.b == doctest::Approx(1.0f).epsilon(0.05f));
        }

        SUBCASE("Cascade 3 Selection (35.0 <= Depth <= 100.0 -> Yellow)") {
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 50.0f, 1.0f); // depth = 50.0
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Cascade 3 is Yellow (1.00, 0.90, 0.10)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.05f));
            CHECK(pix.g == doctest::Approx(0.9f).epsilon(0.05f));
            CHECK(pix.b == doctest::Approx(0.10f).epsilon(0.05f));
        }

        SUBCASE("Cascade Smooth Boundary Blending Invariant") {
            shader->SetInt("u_DebugMode", 24); // Mode 24: Direct shadow factor (1 = lit, 0 = occluded)
            camData.ShadowSettings = glm::ivec4(0, 16, 0, 24); // Hard shadow (1 tap)

            // Layer 0 is lit (depth 1.0). Set layer 1 depth to 0.0f (occluded).
            float zeroDepth = 0.0f;
            glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 1, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &zeroDepth);

            // Both cascade matrices map (0,0,0) to z = 0.5
            camData.LightSpaceMatrices[0] = glm::mat4(1.0f);
            camData.LightSpaceMatrices[1] = glm::mat4(1.0f);

            // Cascade 0 split = 5.0, blendWidth = 0.20 (blendZone = [4.0, 5.0])
            camData.ShadowParams = glm::vec4(0.0f, 0.0f, 0.0f, 0.20f);
            // Camera position at depth = 4.5 -> exact 50% blend between cascade 0 (lit 1.0) and cascade 1 (shadowed 0.0)
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 4.5f, 1.0f);
            gl.UpdateCameraUBO(camData);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Restore layer 1 to 1.0f
            float oneDepth = 1.0f;
            glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 1, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);

            // 50% interpolated blend factor = 0.5
            CHECK(pix.r == doctest::Approx(0.5f).epsilon(0.05f));
        }

        SUBCASE("Far Shadow Distance Fadeout Invariant") {
            shader->SetInt("u_DebugMode", 24); // Shadow Factor (1 = lit, 0 = occluded)
            camData.CameraPosition = glm::vec4(0.0f, 0.0f, 120.0f, 1.0f); // Far beyond 100.0
            camData.ShadowParams   = glm::vec4(0.001f, 0.002f, 0.02f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // Beyond far clip (100.0 + 15.0 = 115.0), shadow factor fade reaches 0.0 (fully unoccluded / lit = 1.0)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.05f));
        }
    }
}
