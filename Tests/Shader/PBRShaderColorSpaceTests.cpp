#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FColorSpace.hpp"
#include <cmath>
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Color Space & Gamma") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU sRGB Decompression for Albedo and Emissive") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const std::string shaderPath = "Engine/Resources/Shaders/PBR_Lit.glsl";
        auto shader = Leon::FShader::Create(shaderPath);
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
        lightData.LightCounts = glm::ivec4(0);
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        // Create mid-gray texture (128/255 ~= 0.50196)
        GLuint midGrayTex = gl.Create1x1SRGBTexture(128, 128, 128, 255);
        float expectedLinear = Leon::SRGBToLinear(128.0f / 255.0f);

        // 1. Albedo Color Space (Mode 14: BaseColor)
        shader->SetInt("u_DebugMode", 14);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, midGrayTex);
        shader->SetInt("u_UseAlbedoMap", 1);
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));

        gl.DrawQuad();
        glm::vec4 albedoOut = gl.ReadPixel();
        CHECK(albedoOut.r == doctest::Approx(expectedLinear).epsilon(0.03f));
        CHECK(albedoOut.g == doctest::Approx(expectedLinear).epsilon(0.03f));
        CHECK(albedoOut.b == doctest::Approx(expectedLinear).epsilon(0.03f));

        // 2. Emissive Color Space (Mode 19: Emissive)
        shader->SetInt("u_DebugMode", 19);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, midGrayTex);
        shader->SetInt("u_UseEmissiveMap", 1);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(1.0f));
        shader->SetFloat("u_EmissiveIntensity", 1.0f);

        gl.DrawQuad();
        glm::vec4 emissiveOut = gl.ReadPixel();
        CHECK(emissiveOut.r == doctest::Approx(expectedLinear).epsilon(0.03f));
        CHECK(emissiveOut.g == doctest::Approx(expectedLinear).epsilon(0.03f));
        CHECK(emissiveOut.b == doctest::Approx(expectedLinear).epsilon(0.03f));

        gl.DestroyTexture(midGrayTex);
    }

    TEST_CASE("PBR_Lit.glsl Hardware GPU Linear Preservation for Metallic, Roughness, AO") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const std::string shaderPath = "Engine/Resources/Shaders/PBR_Lit.glsl";
        auto shader = Leon::FShader::Create(shaderPath);
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
        lightData.LightCounts = glm::ivec4(0);
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        GLuint midGrayTex = gl.Create1x1Texture(128, 128, 128, 255);
        float expectedLinear = 128.0f / 255.0f; // ~0.50196 (NO gamma 2.2 exponent applied)

        // Metallic (Mode 15)
        shader->SetInt("u_DebugMode", 15);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, midGrayTex);
        shader->SetInt("u_UseMetallicMap", 1);
        shader->SetFloat("u_Metallic", 1.0f);
        gl.DrawQuad();
        glm::vec4 metOut = gl.ReadPixel();
        CHECK(metOut.r == doctest::Approx(expectedLinear).epsilon(0.02f));

        // Roughness (Mode 16)
        shader->SetInt("u_DebugMode", 16);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, midGrayTex);
        shader->SetInt("u_UseRoughnessMap", 1);
        shader->SetFloat("u_Roughness", 1.0f);
        gl.DrawQuad();
        glm::vec4 roughOut = gl.ReadPixel();
        CHECK(roughOut.r == doctest::Approx(expectedLinear).epsilon(0.02f));

        // AO (Mode 18)
        shader->SetInt("u_DebugMode", 18);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, midGrayTex);
        shader->SetInt("u_UseAOMap", 1);
        shader->SetFloat("u_AO", 1.0f);
        shader->SetFloat("u_OcclusionStrength", 1.0f);
        gl.DrawQuad();
        glm::vec4 aoOut = gl.ReadPixel();
        CHECK(aoOut.r == doctest::Approx(expectedLinear).epsilon(0.02f));

        gl.DestroyTexture(midGrayTex);
    }
}
