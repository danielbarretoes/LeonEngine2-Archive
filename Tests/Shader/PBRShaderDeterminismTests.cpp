#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Hardware Determinism") {

    TEST_CASE("Repeatable 100% Deterministic GPU Shading across Multiple Runs") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping shader GPU test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightData.LightCounts.x      = 0;
        lightData.LightCounts.y      = 0;
        lightData.EnvSkyColor        = glm::vec4(0.0f); // Zero ambient to test direct Lo accumulation
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_DebugMode", 0);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(0.8f, 0.5f, 0.2f));
        shader->SetFloat("u_Metallic", 0.5f);
        shader->SetFloat("u_Roughness", 0.3f);

        gl.DrawQuad();
        glm::vec4 firstPass = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(firstPass.r));
        CHECK(firstPass.r > 0.05f); // Validates that hdrColor = ambient + Lo + emissive accumulates Lo in normal mode (DebugMode=0)

        for (int run = 0; run < 10; ++run) {
            gl.DrawQuad();
            glm::vec4 passResult = gl.ReadPixel(0, 0);

            CHECK(passResult.r == firstPass.r);
            CHECK(passResult.g == firstPass.g);
            CHECK(passResult.b == firstPass.b);
            CHECK(passResult.a == firstPass.a);
        }
    }
}
