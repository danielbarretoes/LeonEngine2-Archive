#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Shadow Multi-Term Depth & Normal Offset Bias") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Multi-Term Bias Mechanics") {
        auto& gl = FHeadlessGLContext::Get();
        if (!gl.IsValid()) return;

        auto shader = FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        SetMat4(shader, "u_Model", glm::mat4(1.0f));
        SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));

        FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.CascadeSplits  = glm::vec4(5.0f, 15.0f, 35.0f, 100.0f);
        camData.ShadowSettings = glm::ivec4(0, 16, 0, 24); // Hard shadow (1 tap), DebugMode = 24 (Shadow factor)

        FLightingBufferData lightData;
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        shader->SetInt("u_UseShadows", 1);
        shader->SetInt("u_DebugMode", 24);

        // Clear layer 0 depth to 0.5f (occluder plane at Z = 0.5)
        float halfDepth = 0.5f;
        glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 0, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &halfDepth);

        SUBCASE("Constant Bias Boundary Shifting") {
            // Light head-on (NdotL = 1.0, slopeFactor = 0.0)
            lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
            gl.UpdateLightingUBO(lightData);

            // deltaZ = 0.0010 -> projCoords.z = 0.5005 (without bias: 0.5005 > 0.5 occluded)
            glm::mat4 lightMat = glm::mat4(1.0f);
            lightMat[3][2] = 0.0010f;
            for (int c = 0; c < 4; ++c) camData.LightSpaceMatrices[c] = lightMat;

            // With constBias = 0.001f, currentDepth = 0.5005 - 0.0010 = 0.4995 < 0.5 -> Lit (1.0)
            camData.ShadowParams = glm::vec4(0.001f, 0.0f, 0.0f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("Slope Scale Bias Active Shifting at Grazing Angles") {
            // Grazing angle (NdotL = 0.7071, slopeFactor = 0.2929)
            lightData.DirLight.Direction = glm::normalize(glm::vec4(0.7071f, 0.0f, -0.7071f, 1.0f));
            gl.UpdateLightingUBO(lightData);

            // deltaZ = 0.0040 -> projCoords.z = 0.5020 (without slope bias: 0.5020 > 0.5 occluded)
            glm::mat4 lightMat = glm::mat4(1.0f);
            lightMat[3][2] = 0.0040f;
            for (int c = 0; c < 4; ++c) camData.LightSpaceMatrices[c] = lightMat;

            // constBias = 0.0001, slopeBias = 0.015 -> bias = 0.0001 + 0.015 * 0.2929 = 0.00449
            // currentDepth = 0.5020 - 0.00449 = 0.49751 < 0.5 -> Lit (1.0)
            camData.ShadowParams = glm::vec4(0.0001f, 0.015f, 0.0f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        SUBCASE("Normal Offset Bias Surface Shifting") {
            // Grazing angle (slopeFactor > 0)
            lightData.DirLight.Direction = glm::normalize(glm::vec4(0.7071f, 0.0f, -0.7071f, 1.0f));
            gl.UpdateLightingUBO(lightData);

            // Light matrix maps normal offset along +Z to decreasing depth in light space:
            glm::mat4 lightMat = glm::mat4(1.0f);
            lightMat[2][2] = -1.0f;
            lightMat[3][2] = 0.0030f;
            for (int c = 0; c < 4; ++c) camData.LightSpaceMatrices[c] = lightMat;

            // constBias = 0.0, slopeBias = 0.0, normalBias = 0.05
            // Normal offset shifts depth < 0.5 -> Lit (1.0)
            camData.ShadowParams = glm::vec4(0.0f, 0.0f, 0.05f, 0.0f);
            gl.UpdateCameraUBO(camData);

            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.02f));
        }

        // Restore layer 0 to 1.0f depth
        float oneDepth = 1.0f;
        glClearTexSubImage(gl.GetDefaultShadowArrayTex(), 0, 0, 0, 0, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);
    }
}
