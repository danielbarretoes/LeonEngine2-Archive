#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Planar Reflections") {

    TEST_CASE("PBR_Lit.glsl Real-Time Planar Reflection Integration (Toggle 0 vs 1)") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        Leon::TestGPU::ApplyFloorView(camData);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        // Metallic mirror: Fresnel≈1 so planar radiance replaces IBL specular (not BRDF LUT).
        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", Leon::TestGPU::FloorFacingNormalMatrix());
        Leon::TestGPU::SetMat4(shader, "u_PlanarViewProjection", glm::mat4(1.0f));
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_DebugMode", 0);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.05f);

        // HDR planar sample on unit 5 (strong blue channel)
        GLuint planarTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &planarTex);
        glTextureStorage2D(planarTex, 1, GL_RGBA16F, 1, 1);
        float blueHdr[4] = {0.2f, 0.2f, 6.0f, 1.0f};
        glTextureSubImage2D(planarTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, blueHdr);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, planarTex);

        // 1. Planar reflection disabled
        shader->SetInt("u_UsePlanarReflection", 0);
        gl.DrawQuad();
        glm::vec4 pixelDisabled = gl.ReadPixel(0, 0);

        // 2. Planar reflection enabled
        shader->SetInt("u_UsePlanarReflection", 1);
        gl.DrawQuad();
        glm::vec4 pixelEnabled = gl.ReadPixel(0, 0);

        CHECK(!std::isnan(pixelEnabled.r));
        CHECK(!std::isnan(pixelEnabled.b));
        CHECK(!std::isinf(pixelEnabled.b));
        // Planar mix must pull specular toward the blue HDR sample
        CHECK(pixelEnabled.b > pixelDisabled.b);
        CHECK(pixelEnabled.b > 1.0f);

        glDeleteTextures(1, &planarTex);
    }

    TEST_CASE("planar UV is projected world position, not screen UV") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();
        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        Leon::TestGPU::ApplyFloorView(camData);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData{};
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", Leon::TestGPU::FloorFacingNormalMatrix());
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UsePlanarReflection", 1);
        shader->SetInt("u_DebugMode", 13);
        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.04f);
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));

        GLuint planarTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &planarTex);
        glTextureStorage2D(planarTex, 1, GL_RGBA16F, 2, 1);
        float texels[8] = {8.0f, 0.1f, 0.1f, 1.0f, 0.1f, 0.1f, 8.0f, 1.0f};
        glTextureSubImage2D(planarTex, 0, 0, 0, 2, 1, GL_RGBA, GL_FLOAT, texels);
        glTextureParameteri(planarTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(planarTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(planarTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(planarTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, planarTex);

        // Origin maps to NDC x=-0.5 → UV.x=0.25 (left / red). Screen UV of this 1x1 FBO is 0.5 (right / blue).
        glm::mat4 planarVP(1.0f);
        planarVP[3][0] = -0.5f;
        Leon::TestGPU::SetMat4(shader, "u_PlanarViewProjection", planarVP);

        gl.DrawQuad();
        glm::vec4 pixel = gl.ReadPixel(0, 0);
        CHECK(pixel.r > 4.0f);
        CHECK(pixel.b < 2.0f);

        glDeleteTextures(1, &planarTex);
    }

    TEST_CASE("planar Li does not replace IBL on meshes lifted off the capture plane") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindDefaultTextures();
        gl.BindFramebuffer(1, 1);

        Leon::FCameraBufferData camData;
        camData.ViewProjection = glm::mat4(1.0f);
        Leon::TestGPU::ApplyFloorView(camData);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        glm::mat4 lifted = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
        Leon::TestGPU::SetMat4(shader, "u_Model", lifted);
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", Leon::TestGPU::FloorFacingNormalMatrix());
        Leon::TestGPU::SetMat4(shader, "u_PlanarViewProjection", glm::mat4(1.0f));
        shader->SetFloat("u_PlanarPlaneDistance", 0.0f);
        shader->SetInt("u_UseIBL", 0);
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_DebugMode", 0);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 1.0f);
        shader->SetFloat("u_Roughness", 0.05f);

        GLuint planarTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &planarTex);
        glTextureStorage2D(planarTex, 1, GL_RGBA16F, 1, 1);
        float blueHdr[4] = {0.2f, 0.2f, 6.0f, 1.0f};
        glTextureSubImage2D(planarTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, blueHdr);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, planarTex);

        shader->SetInt("u_UsePlanarReflection", 0);
        gl.DrawQuad();
        glm::vec4 pixelIbl = gl.ReadPixel(0, 0);

        shader->SetInt("u_UsePlanarReflection", 1);
        gl.DrawQuad();
        glm::vec4 pixelLifted = gl.ReadPixel(0, 0);

        CHECK(std::abs(pixelLifted.b - pixelIbl.b) < 0.15f);
        CHECK(pixelLifted.b < 2.0f);

        glDeleteTextures(1, &planarTex);
    }
}
