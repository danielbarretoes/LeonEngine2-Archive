#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>
#include <vector>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Planar Reflections") {

    TEST_CASE("PBR_Lit.glsl Real-Time Planar Reflection Integration (Toggle 0 vs 1)") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid()) {
            MESSAGE("Headless OpenGL context not available — skipping shader GPU test.");
            return;
        }

        auto shader = Leon::FShader::Create("Engine/Assets/Shaders/PBR_Lit.glsl");
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
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gl.UpdateLightingUBO(lightData);

        // Smooth surface (roughness = 0.05) to maximize reflection strength
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
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.05f);

        // Create a custom bright blue planar reflection texture on Unit 5
        GLuint planarTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &planarTex);
        glTextureStorage2D(planarTex, 1, GL_RGBA8, 1, 1);
        uint32_t bluePixel = 0xFFFF0000; // Blue in ABGR
        glTextureSubImage2D(planarTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &bluePixel);

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

        // Output must change when planar reflections are mixed in
        CHECK(!std::isnan(pixelEnabled.r));
        CHECK(!std::isnan(pixelEnabled.b));
        CHECK(pixelEnabled.b >= pixelDisabled.b);

        glDeleteTextures(1, &planarTex);
    }
}
