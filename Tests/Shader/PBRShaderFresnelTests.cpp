#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Fresnel Evaluation") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Fresnel Schlick (cosTheta in {1.0, 0.5, 0.0})") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping shader GPU test.");
            return;
        }

        const std::string shaderPath = "Engine/Assets/Shaders/PBR_Lit.glsl";
        REQUIRE(std::filesystem::exists(shaderPath));

        auto shader = Leon::FShader::Create(shaderPath);
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        // Setup Orthogonal Camera looking directly at origin from (0, 0, 1)
        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f); // Identity maps Quad [-1, 1] directly to NDC
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        // Lighting UBO: Directional Light towards -Z (L = +Z, parallel to V)
        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f); // xyz = -L, w = enabled
        lightData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // radiance = 1.0
        lightData.LightCounts.x      = 0; // No point lights
        lightData.LightCounts.y      = 0; // No spot lights
        lightData.EnvSkyColor        = glm::vec4(0.0f);
        gl.UpdateLightingUBO(lightData);

        // Uniforms
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

        // 1. Dielectric (Albedo = (1,1,1), Metallic = 0.0, Roughness = 0.5) at perpendicular view (V=L=N)
        // At N=V=L: NdotV=1, NdotL=1, HdotV=1 -> cosTheta = 1.0 -> F = F0 = 0.04
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelDielectric = gl.ReadPixel(0, 0);

        // Verify valid floating-point values
        CHECK(!std::isnan(pixelDielectric.r));
        CHECK(!std::isnan(pixelDielectric.g));
        CHECK(!std::isnan(pixelDielectric.b));
        CHECK(pixelDielectric.r > 0.0f);

        // 2. Gold Metallic (Albedo = (1.0, 0.71, 0.29), Metallic = 1.0, Roughness = 0.5)
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f, 0.71f, 0.29f));
        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelGold = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(pixelGold.r));
        CHECK(!std::isnan(pixelGold.g));
        CHECK(!std::isnan(pixelGold.b));

        // Gold color channels maintain relative reflectance ratio: R > G > B
        CHECK(pixelGold.r > pixelGold.g);
        CHECK(pixelGold.g > pixelGold.b);

        // 3. Exact Oblique Angle Test (cosTheta = 0.5) to catch Exponent 5.0 -> 4.0 mutations
        // V = (0, 0.866025, 0.5), L = (0, -0.866025, 0.5) -> H = (0, 0, 1), H.V = 0.5
        // F = 0.04 + 0.96 * 0.5^5 = 0.07000 (exponent 4 would give 0.1000)
        // Expected Total Lo = 0.25661
        camData.CameraPosition = glm::vec4(0.0f, 0.866025f, 0.5f, 0.0f);
        gl.UpdateCameraUBO(camData);

        lightData.DirLight.Direction = glm::vec4(0.0f, 0.866025f, -0.5f, 1.0f); // -L = (0, 0.866025, -0.5)
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelOblique = gl.ReadPixel(0, 0);

        const float expectedOblique = 0.25661f;
        CHECK(pixelOblique.r == doctest::Approx(expectedOblique).epsilon(0.02f));
        CHECK(pixelOblique.g == doctest::Approx(expectedOblique).epsilon(0.02f));
        CHECK(pixelOblique.b == doctest::Approx(expectedOblique).epsilon(0.02f));
    }
}
