#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Normal Mapping") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Flat Normal Map Invariance") {
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

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward  = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
        lightData.DirLight.Color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        lightData.LightCounts        = glm::ivec4(0);
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetFloat("u_NormalScale", 1.0f);
        shader->SetFloat("u_OcclusionStrength", 1.0f);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));
        shader->SetFloat("u_EmissiveIntensity", 0.0f);

        // Debug Mode 17: Normal (N * 0.5 + 0.5)
        shader->SetInt("u_DebugMode", 17);

        // 1. Without Normal Map: N = (0, 0, 1) -> Color = (0.5, 0.5, 1.0)
        shader->SetInt("u_UseNormalMap", 0);
        gl.DrawQuad();
        glm::vec4 geomNorm = gl.ReadPixel();
        CHECK(geomNorm.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(geomNorm.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(geomNorm.b == doctest::Approx(1.0f).epsilon(0.02f));

        // 2. With Flat Normal Map (128, 128, 255): N must remain identical (0.5, 0.5, 1.0)
        GLuint flatTex = gl.Create1x1Texture(128, 128, 255, 255);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, flatTex);
        shader->SetInt("u_UseNormalMap", 1);
        gl.DrawQuad();
        glm::vec4 mapNorm = gl.ReadPixel();

        CHECK(mapNorm.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(mapNorm.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(mapNorm.b == doctest::Approx(1.0f).epsilon(0.02f));

        gl.DestroyTexture(flatTex);
    }

    TEST_CASE("PBR_Lit.glsl Hardware GPU Normal Map Perturbation and NormalScale") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) return;

        const std::string shaderPath = "Engine/Assets/Shaders/PBR_Lit.glsl";
        auto shader = Leon::FShader::Create(shaderPath);
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
        lightData.LightCounts        = glm::ivec4(0);
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));
        shader->SetInt("u_DebugMode", 17); // Normal Output

        // Create +X tilted normal map: (255, 128, 255) -> TS normal ~= (1.0, 0.0, 1.0) normalized -> (+0.707, 0, +0.707)
        GLuint tiltXTex = gl.Create1x1Texture(255, 128, 255, 255);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tiltXTex);
        shader->SetInt("u_UseNormalMap", 1);

        // Scale = 1.0: N.x must be significantly positive (> 0.5)
        shader->SetFloat("u_NormalScale", 1.0f);
        gl.DrawQuad();
        glm::vec4 normScale1 = gl.ReadPixel();
        CHECK(normScale1.r > 0.70f); // Mapped: (0.707 * 0.5 + 0.5) ~= 0.85
        CHECK(normScale1.g == doctest::Approx(0.5f).epsilon(0.03f));

        // Scale = 0.0: N.x must fall back completely to geometric flat (0.5)
        shader->SetFloat("u_NormalScale", 0.0f);
        gl.DrawQuad();
        glm::vec4 normScale0 = gl.ReadPixel();
        CHECK(normScale0.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(normScale0.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(normScale0.b == doctest::Approx(1.0f).epsilon(0.02f));

        // Scale = 0.5: N.x must be intermediate between 0.5 and normScale1.r
        shader->SetFloat("u_NormalScale", 0.5f);
        gl.DrawQuad();
        glm::vec4 normScaleHalf = gl.ReadPixel();
        CHECK(normScaleHalf.r > 0.55f);
        CHECK(normScaleHalf.r < normScale1.r);

        // Fallback when u_UseNormalMap is 0: must ignore bound texture
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetFloat("u_NormalScale", 1.0f);
        gl.DrawQuad();
        glm::vec4 fallbackNorm = gl.ReadPixel();
        CHECK(fallbackNorm.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(fallbackNorm.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(fallbackNorm.b == doctest::Approx(1.0f).epsilon(0.02f));

        gl.DestroyTexture(tiltXTex);
    }

}
