#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"

using namespace Leon;
using namespace Leon::TestGPU;

TEST_SUITE("Shader GPU - Point cubemap shadows") {

    TEST_CASE("PBR_Lit.glsl point shadow factor lit vs occluded") {
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
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UsePointShadows", 1);
        shader->SetInt("u_DebugMode", 40);

        FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        camData.ShadowSettings = glm::ivec4(0, 0, 4, 40);
        camData.ShadowParams = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        lightData.PointLights[0].Position = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
        lightData.PointLights[0].Color = glm::vec4(1.0f, 1.0f, 1.0f, 8.0f);
        lightData.PointLights[0].Params = glm::vec4(10.0f, 0.0f, 0.0f, 0.0f);
        lightData.LightCounts = glm::ivec4(1, 0, 1, 0);
        gl.UpdateLightingUBO(lightData);

        SUBCASE("Stored depth 1 is lit") {
            float oneDepth = 1.0f;
            glClearTexImage(gl.GetDefaultPointCubeArrayTex(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);
            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.05f));
        }

        SUBCASE("Stored depth 0 is occluded") {
            float zeroDepth = 0.0f;
            glClearTexImage(gl.GetDefaultPointCubeArrayTex(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, &zeroDepth);
            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            float oneDepth = 1.0f;
            glClearTexImage(gl.GetDefaultPointCubeArrayTex(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);
            CHECK(pix.r == doctest::Approx(0.0f).epsilon(0.05f));
        }

        SUBCASE("Linear bias prevents self-hit acne when stored equals raw depth") {
            // Fragment at z=0, light at z=2, radius=10 → raw linear depth = 0.2.
            // Stored 0.195 mimics a self-hit without polygon offset; bias must keep it lit.
            float storedDepth = 0.195f;
            glClearTexImage(gl.GetDefaultPointCubeArrayTex(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, &storedDepth);
            gl.DrawQuad();
            glm::vec4 pix = gl.ReadPixel(0, 0);

            float oneDepth = 1.0f;
            glClearTexImage(gl.GetDefaultPointCubeArrayTex(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);
            CHECK(pix.r == doctest::Approx(1.0f).epsilon(0.08f));
        }
    }
}
