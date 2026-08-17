#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Cook-Torrance Evaluation") {

    TEST_CASE("PBR_Lit.glsl Exact Hardware GPU Numerical Output vs Analytical Formula") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const std::string shaderPath = "Engine/Assets/Shaders/PBR_Lit.glsl";
        REQUIRE(std::filesystem::exists(shaderPath));

        auto shader = Leon::FShader::Create(shaderPath);
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f); // L = +Z
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);      // radiance = 1.0
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(0.0f); // Zero ambient for pure direct lighting test
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
        shader->SetInt("u_DebugMode", 10);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));

        // 1. Dielectric Rough: Albedo = (1, 1, 1), Metallic = 0.0, Roughness = 0.5
        // Analytical Expectation:
        // D = 5.092958, G = 1.0, F = 0.04 -> Specular = 5.092958 * 0.04 / 4 = 0.05093
        // kD = 0.96 -> Diffuse = 0.96 / PI = 0.305577
        // Total Expected = 0.305577 + 0.05093 = 0.356507
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelDielectricRough = gl.ReadPixel(0, 0);

        const float expectedDielectric = 0.356507f;
        CHECK(pixelDielectricRough.r == doctest::Approx(expectedDielectric).epsilon(0.015f));
        CHECK(pixelDielectricRough.g == doctest::Approx(expectedDielectric).epsilon(0.015f));
        CHECK(pixelDielectricRough.b == doctest::Approx(expectedDielectric).epsilon(0.015f));

        // 2. Pure Metal Rough: Albedo = (1.0, 0.71, 0.29), Metallic = 1.0, Roughness = 0.5
        // Analytical Expectation:
        // kD = 0.0 (Diffuse is strictly zero!)
        // F0 = (1.0, 0.71, 0.29)
        // Specular_R = 5.092958 * 1.00 / 4 = 1.27324
        // Specular_G = 5.092958 * 0.71 / 4 = 0.90400
        // Specular_B = 5.092958 * 0.29 / 4 = 0.36924
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f, 0.71f, 0.29f));
        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelGoldRough = gl.ReadPixel(0, 0);

        CHECK(pixelGoldRough.r == doctest::Approx(1.27324f).epsilon(0.02f));
        CHECK(pixelGoldRough.g == doctest::Approx(0.90400f).epsilon(0.02f));
        CHECK(pixelGoldRough.b == doctest::Approx(0.36924f).epsilon(0.02f));
    }
}
