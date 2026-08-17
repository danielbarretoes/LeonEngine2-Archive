#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Shadow Layer & Atlas Mapping") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU 4-Layer Cascade Array Depth Sampling") {
        auto& gl = FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

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
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.CascadeSplits = glm::vec4(5.0f, 15.0f, 35.0f, 100.0f);
        camData.ShadowSettings = glm::ivec4(1, 16, 0, 0);

        for (int c = 0; c < 4; ++c) {
            camData.LightSpaceMatrices[c] = glm::mat4(1.0f);
        }

        FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        gl.UpdateCameraUBO(camData);
        gl.UpdateLightingUBO(lightData);

        shader->SetInt("u_UseShadows", 1);

        // Check depth debug views for all 4 cascades
        for (int c = 0; c < 4; ++c) {
            shader->SetInt("u_DebugMode", 27 + c);
            gl.DrawQuad();
            glm::vec4 depthPix = gl.ReadPixel(0, 0);
            CHECK(depthPix.r >= 0.0f);
            CHECK(depthPix.r <= 1.0f);
        }
    }
}
