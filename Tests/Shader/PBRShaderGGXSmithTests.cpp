#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>
#include <vector>

TEST_SUITE("Shader GPU - PBR_Lit.glsl GGX, Smith & Material Grid") {

    TEST_CASE("GGX NDF Hardware On-Axis Monotonic Scaling with Roughness") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
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
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f); // L = +Z (on axis)
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(0.0f);
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
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 1.0f); // Pure specular to isolate NDF

        std::vector<float> roughnesses = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        std::vector<float> onAxisSpecular;

        for (float r : roughnesses) {
            shader->SetFloat("u_Roughness", r);
            gl.DrawQuad();
            glm::vec4 pixel = gl.ReadPixel(0, 0);

            CHECK(!std::isnan(pixel.r));
            CHECK(!std::isinf(pixel.r));
            CHECK(pixel.r > 0.0f);
            onAxisSpecular.push_back(pixel.r);
        }

        // On-axis GGX specular intensity must decrease strictly monotonically as roughness increases
        for (size_t i = 1; i < onAxisSpecular.size(); ++i) {
            CHECK(onAxisSpecular[i - 1] > onAxisSpecular[i]);
        }

        // 2. Exact Smith G1*G2 Oblique Masking Test (NdotV = 0.5, NdotL = 0.5, roughness = 0.5)
        // D = 5.092958, G1 = 0.780488 -> G = G1*G2 = 0.609161
        // Specular = 5.092958 * 0.609161 / (4 * 0.5 * 0.5) = 3.10243
        // Lo = 3.10243 * 0.5 = 1.55121 (if G2 is dropped, Lo would be 1.98750 -> 28% error)
        camData.CameraPosition = glm::vec4(0.0f, 0.866025f, 0.5f, 0.0f);
        gl.UpdateCameraUBO(camData);

        lightData.DirLight.Direction = glm::vec4(0.0f, 0.866025f, -0.5f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        gl.DrawQuad();
        glm::vec4 pixelSmith = gl.ReadPixel(0, 0);

        CHECK(pixelSmith.r == doctest::Approx(1.55121f).epsilon(0.02f));
    }

    TEST_CASE("Comprehensive 7-Material Parameter Matrix in Real Hardware") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
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
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(0.0f);
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
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(0.9f, 0.7f, 0.3f));

        struct FMatCase {
            float metallic;
            float roughness;
        };
        std::vector<FMatCase> testCases = {
            {0.0f, 0.05f}, // 1. Metallic = 0, Roughness = 0.05
            {1.0f, 0.05f}, // 2. Metallic = 1, Roughness = 0.05
            {0.0f, 0.50f}, // 3. Metallic = 0, Roughness = 0.50
            {1.0f, 0.50f}, // 4. Metallic = 1, Roughness = 0.50
            {0.5f, 0.25f}, // 5. Metallic = 0.5, Roughness = 0.25
            {0.0f, 1.00f}, // 6. Roughness = 1.0
            {1.0f, 1.00f}  // 7. Roughness = 1.0 Metal
        };

        for (const auto& mc : testCases) {
            shader->SetFloat("u_Metallic", mc.metallic);
            shader->SetFloat("u_Roughness", mc.roughness);

            gl.DrawQuad();
            glm::vec4 pixel = gl.ReadPixel(0, 0);

            CHECK(!std::isnan(pixel.r));
            CHECK(!std::isnan(pixel.g));
            CHECK(!std::isnan(pixel.b));
            CHECK(pixel.r >= 0.0f);
            CHECK(pixel.g >= 0.0f);
            CHECK(pixel.b >= 0.0f);
        }
    }
}
