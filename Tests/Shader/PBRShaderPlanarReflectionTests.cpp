#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <cmath>
#include <filesystem>
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
        camData.CameraPosition = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        camData.CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        gl.UpdateCameraUBO(camData);

        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        // Metallic mirror: Fresnel≈1 so planar radiance replaces IBL specular (not BRDF LUT).
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
        shader->SetInt("u_DebugMode", 0);
        Leon::TestGPU::SetFloat2(shader, "u_ScreenSize", glm::vec2(1.0f, 1.0f));
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
}
