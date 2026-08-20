#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FColorSpace.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Emissive Radiance") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Emissive Intensity and Color Output") {
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

        // Turn OFF all scene lights and IBL completely
        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f); // Disabled
        lightData.DirLight.Color = glm::vec4(0.0f);
        lightData.LightCounts = glm::ivec4(0);
        lightData.EnvSkyColor = glm::vec4(0.0f);
        lightData.EnvHorizonColor = glm::vec4(0.0f);
        lightData.EnvGroundColor = glm::vec4(0.0f);
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
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(0.5f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);
        shader->SetFloat("u_AO", 1.0f);

        // 1. With Emissive = 0: Output in darkness is strictly (0, 0, 0)
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(1.0f, 0.5f, 0.2f));
        shader->SetFloat("u_EmissiveIntensity", 0.0f);
        shader->SetInt("u_DebugMode", 0); // Full shading

        gl.DrawQuad();
        glm::vec4 zeroEmissive = gl.ReadPixel();
        CHECK(std::abs(zeroEmissive.r) < 0.001f);
        CHECK(std::abs(zeroEmissive.g) < 0.001f);
        CHECK(std::abs(zeroEmissive.b) < 0.001f);

        // 2. With Emissive Intensity = 1.0: Output is exactly u_EmissiveColor
        shader->SetFloat("u_EmissiveIntensity", 1.0f);
        gl.DrawQuad();
        glm::vec4 emissivePixel1 = gl.ReadPixel();
        CHECK(emissivePixel1.r == doctest::Approx(1.0f).epsilon(0.01f));
        CHECK(emissivePixel1.g == doctest::Approx(0.5f).epsilon(0.01f));
        CHECK(emissivePixel1.b == doctest::Approx(0.2f).epsilon(0.01f));

        // 3. HDR Emissive (> 1.0)
        shader->SetFloat("u_EmissiveIntensity", 4.0f);
        gl.DrawQuad();
        glm::vec4 hdrEmissive = gl.ReadPixel();
        CHECK(hdrEmissive.r == doctest::Approx(4.0f).epsilon(0.01f));
        CHECK(hdrEmissive.g == doctest::Approx(2.0f).epsilon(0.01f));
        CHECK(hdrEmissive.b == doctest::Approx(0.8f).epsilon(0.01f));

        // 4. Emissive Texture Map
        GLuint emissiveTex = gl.Create1x1SRGBTexture(255, 128, 0, 255);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, emissiveTex);
        shader->SetInt("u_UseEmissiveMap", 1);
        shader->SetFloat("u_EmissiveIntensity", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(1.0f));

        gl.DrawQuad();
        glm::vec4 mapEmissive = gl.ReadPixel();
        float expectedLinearG = Leon::SRGBToLinear(128.0f / 255.0f);
        CHECK(mapEmissive.r == doctest::Approx(1.0f).epsilon(0.01f));
        CHECK(mapEmissive.g == doctest::Approx(expectedLinearG).epsilon(0.03f));
        CHECK(std::abs(mapEmissive.b) < 0.001f);

        gl.DestroyTexture(emissiveTex);
    }
}
