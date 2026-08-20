#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Direct Lighting & Attenuation") {

    TEST_CASE("Directional Light Angle Response (NdotL in {1.0, 0.5, 0.0})") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const std::string shaderPath = "Engine/Resources/Shaders/PBR_Lit.glsl";
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
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        Leon::FLightingBufferData lightData;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(0.0f); // Zero ambient for pure direct light test

        // 1. Perpendicular: L = (0, 0, 1) -> NdotL = 1.0
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        gl.DrawQuad();
        glm::vec4 pixelPerp = gl.ReadPixel(0, 0);

        // 2. Slanted at 60 degrees: L = (0, 0.866025, 0.5) -> NdotL = 0.50 -> Output must scale by exactly 0.5x
        lightData.DirLight.Direction = glm::vec4(0.0f, -0.866025f, -0.5f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        gl.DrawQuad();
        glm::vec4 pixelSlanted = gl.ReadPixel(0, 0);

        // 3. Back-facing: L = (0, 0, -1) -> NdotL <= 0.0 -> Output must be strictly zero!
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        gl.DrawQuad();
        glm::vec4 pixelBack = gl.ReadPixel(0, 0);

        CHECK(pixelPerp.r > 0.1f);
        CHECK(pixelSlanted.r == doctest::Approx(0.154546f).epsilon(0.01f));
        CHECK(pixelSlanted.r < pixelPerp.r);
        CHECK(pixelBack.r == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelBack.g == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelBack.b == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    TEST_CASE("Point Light UE4 Inverse-Square Radius Attenuation Response") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Resources/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

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
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f; // Disable directional light
        lightData.LightCounts.x = 1;           // 1 Point Light
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(0.0f); // Zero ambient for pure direct light test

        // Point Light with radius = 10.0
        lightData.PointLights[0].Color = glm::vec4(1.0f, 1.0f, 1.0f, 10.0f);
        lightData.PointLights[0].Params = glm::vec4(10.0f, 0.0f, 0.0f, 0.0f);

        // Distance d = 1.0: Position = (0, 0, 1)
        lightData.PointLights[0].Position = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);
        gl.DrawQuad();
        glm::vec4 pixelDist1 = gl.ReadPixel(0, 0);

        // Distance d = 2.0: Position = (0, 0, 2)
        lightData.PointLights[0].Position = glm::vec4(0.0f, 0.0f, 2.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);
        gl.DrawQuad();
        glm::vec4 pixelDist2 = gl.ReadPixel(0, 0);

        // Distance d = 12.0 (Beyond radius 10.0): Position = (0, 0, 12)
        lightData.PointLights[0].Position = glm::vec4(0.0f, 0.0f, 12.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);
        gl.DrawQuad();
        glm::vec4 pixelBeyondRadius = gl.ReadPixel(0, 0);

        // Monotonic attenuation: pixelDist1 > pixelDist2 > pixelBeyondRadius == 0
        CHECK(pixelDist1.r > pixelDist2.r);
        CHECK(pixelDist2.r > pixelBeyondRadius.r);
        CHECK(pixelBeyondRadius.r == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    TEST_CASE("Spot Light Conical Cutoff and Smoothstep Penumbra Response") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Resources/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseSpotShadows", 0);
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
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 1;             // 1 Spot Light
        lightData.EnvSkyColor = glm::vec4(0.0f); // Zero ambient for pure direct light test

        // Spot Light placed at (0, 0, 1) looking down -Z
        // cutOff = cos(15 deg) = 0.9659, outerCutOff = cos(30 deg) = 0.8660
        lightData.SpotLights[0].Position = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        lightData.SpotLights[0].Direction = glm::vec4(0.0f, 0.0f, -1.0f, 0.9659f);
        lightData.SpotLights[0].Color = glm::vec4(1.0f, 1.0f, 1.0f, 0.8660f);
        lightData.SpotLights[0].Params = glm::vec4(10.0f, 10.0f, 0.0f, 0.0f); // radius=10, intensity=10

        // 1. Center of cone: Direct hit (L is on axis)
        gl.UpdateLightingUBO(lightData);
        gl.DrawQuad();
        glm::vec4 pixelInsideCone = gl.ReadPixel(0, 0);

        // 2. Pointing away from surface: Spot Direction = (+1, 0, 0)
        lightData.SpotLights[0].Direction = glm::vec4(1.0f, 0.0f, 0.0f, 0.9659f);
        gl.UpdateLightingUBO(lightData);
        gl.DrawQuad();
        glm::vec4 pixelOutsideCone = gl.ReadPixel(0, 0);

        CHECK(pixelInsideCone.r > 0.1f);
        CHECK(pixelOutsideCone.r == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelOutsideCone.g == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(pixelOutsideCone.b == doctest::Approx(0.0f).epsilon(1e-5f));
    }
}
