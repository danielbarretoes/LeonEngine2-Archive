#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FColorSpace.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Material Textures & Fallbacks") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Albedo Map & Scalar Combination") {
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
        shader->SetFloat("u_NormalScale", 1.0f);
        shader->SetFloat("u_OcclusionStrength", 1.0f);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        // Mode 14: BaseColor / Albedo
        shader->SetInt("u_DebugMode", 14);

        // 1. Pure scalar albedo (0.8, 0.4, 0.2), texture disabled
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(0.8f, 0.4f, 0.2f));
        shader->SetInt("u_UseAlbedoMap", 0);
        gl.DrawQuad();
        glm::vec4 scalarAlbedo = gl.ReadPixel();
        CHECK(scalarAlbedo.r == doctest::Approx(0.8f).epsilon(0.01f));
        CHECK(scalarAlbedo.g == doctest::Approx(0.4f).epsilon(0.01f));
        CHECK(scalarAlbedo.b == doctest::Approx(0.2f).epsilon(0.01f));

        // 2. Albedo map enabled with 0.5 sRGB (188/255 -> 0.51 linear)
        // 255 in 8-bit = 1.0 linear, 128 in 8-bit ~= (128/255)^2.2 ~= 0.218 linear
        GLuint albedoTex = gl.Create1x1SRGBTexture(128, 255, 0, 255);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedoTex);
        shader->SetInt("u_UseAlbedoMap", 1);
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));

        gl.DrawQuad();
        glm::vec4 texAlbedo = gl.ReadPixel();
        float expectedLinearR = Leon::SRGBToLinear(128.0f / 255.0f);
        CHECK(texAlbedo.r == doctest::Approx(expectedLinearR).epsilon(0.03f));
        CHECK(texAlbedo.g == doctest::Approx(1.0f).epsilon(0.01f));
        CHECK(texAlbedo.b == doctest::Approx(0.0f).epsilon(0.01f));

        // 3. Modulate with scalar tint (0.5, 0.5, 0.5)
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(0.5f));
        gl.DrawQuad();
        glm::vec4 tintedAlbedo = gl.ReadPixel();
        CHECK(tintedAlbedo.r == doctest::Approx(expectedLinearR * 0.5f).epsilon(0.03f));
        CHECK(tintedAlbedo.g == doctest::Approx(0.5f).epsilon(0.01f));

        gl.DestroyTexture(albedoTex);
    }

    TEST_CASE("PBR_Lit.glsl Hardware GPU Metallic, Roughness and AO Map Channels") {
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

        // Test Metallic (Mode 15)
        shader->SetInt("u_DebugMode", 15);
        GLuint metTex = gl.Create1x1Texture(178, 0, 0, 255); // 178/255 ~= 0.70
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, metTex);
        shader->SetInt("u_UseMetallicMap", 1);
        shader->SetFloat("u_Metallic", 1.0f);
        gl.DrawQuad();
        glm::vec4 metPixel = gl.ReadPixel();
        CHECK(metPixel.r == doctest::Approx(178.0f / 255.0f).epsilon(0.02f));

        // Test Roughness (Mode 16)
        shader->SetInt("u_DebugMode", 16);
        GLuint roughTex = gl.Create1x1Texture(89, 0, 0, 255); // 89/255 ~= 0.35
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, roughTex);
        shader->SetInt("u_UseRoughnessMap", 1);
        shader->SetFloat("u_Roughness", 1.0f);
        gl.DrawQuad();
        glm::vec4 roughPixel = gl.ReadPixel();
        CHECK(roughPixel.r == doctest::Approx(89.0f / 255.0f).epsilon(0.02f));

        // Test AO & Occlusion Strength (Mode 18)
        shader->SetInt("u_DebugMode", 18);
        GLuint aoTex = gl.Create1x1Texture(51, 0, 0, 255); // 51/255 ~= 0.20
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, aoTex);
        shader->SetInt("u_UseAOMap", 1);
        shader->SetFloat("u_AO", 1.0f);

        // Strength 1.0 -> AO = 0.20
        shader->SetFloat("u_OcclusionStrength", 1.0f);
        gl.DrawQuad();
        glm::vec4 aoPixel1 = gl.ReadPixel();
        CHECK(aoPixel1.r == doctest::Approx(51.0f / 255.0f).epsilon(0.02f));

        // Strength 0.0 -> AO = 1.0 (Occlusion disabled)
        shader->SetFloat("u_OcclusionStrength", 0.0f);
        gl.DrawQuad();
        glm::vec4 aoPixel0 = gl.ReadPixel();
        CHECK(aoPixel0.r == doctest::Approx(1.0f).epsilon(0.01f));

        gl.DestroyTexture(metTex);
        gl.DestroyTexture(roughTex);
        gl.DestroyTexture(aoTex);
    }
}
