#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Shadow Screen-Space Contact Shadows") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Screen-Space Contact Shadows") {
        auto& gl = FHeadlessGLContext::Get();
        if (!gl.IsValid()) return;

        auto shader = FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        SetMat4(shader, "u_Model", glm::mat4(1.0f));
        SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));

        shader->SetInt("u_UseShadows", 1);
        shader->SetInt("u_DebugMode", 26); // Mode 26: Contact Shadow Factor

        FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.CascadeSplits  = glm::vec4(5.0f, 15.0f, 35.0f, 100.0f);
        camData.ShadowParams   = glm::vec4(0.001f, 0.002f, 0.02f, 0.0f);
        camData.ContactShadowParams = glm::vec4(0.35f, 0.05f, 0.0f, 0.0f);

        for (int c = 0; c < 4; ++c) camData.LightSpaceMatrices[c] = glm::mat4(1.0f);

        FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        SUBCASE("Contact Shadows Disabled Baseline") {
            camData.ShadowSettings = glm::ivec4(1, 16, 0, 26); // bContactShadows = 0
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // When disabled, contact shadow factor must be 1.0 (unoccluded)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.01f));
        }

        SUBCASE("Contact Shadows Enabled Unoccluded Baseline") {
            camData.ShadowSettings = glm::ivec4(1, 16, 1, 26); // bContactShadows = 1
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            // When enabled with clear ray path, shadowFactor must return 1.0 (unoccluded)
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("Contact Shadow Multiplicative Integration with Directional Shadow") {
            shader->SetInt("u_DebugMode", 10); // Mode 10: Direct Radiance Lo
            shader->SetInt("u_UseAlbedoMap", 0);
            shader->SetInt("u_UseIBL", 0);
            SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
            shader->SetFloat("u_Metallic", 0.0f);
            shader->SetFloat("u_Roughness", 0.5f);
            shader->SetFloat("u_AO", 1.0f);
            SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));

            camData.ShadowSettings = glm::ivec4(1, 16, 1, 10); // bContactShadows = 1
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 hdrPix = gl.ReadPixel(0, 0);

            // Lit fragment with unoccluded contact shadow should produce direct radiance > 0.0
            CHECK(hdrPix.r > 0.05f);
        }
    }
}
