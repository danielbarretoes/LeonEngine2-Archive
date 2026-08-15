#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>
#include <vector>

TEST_SUITE("Shader GPU - PBR_Lit.glsl IBL Integration") {

    TEST_CASE("PBR_Lit.glsl Real Hardware GPU Image-Based Lighting Pipeline") {
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

        // Disable all direct lights so output is 100% pure indirect IBL ambient
        Leon::FLightingBufferData lightData;
        lightData.DirLight.Direction.w = 0.0f;
        lightData.LightCounts.x = 0;
        lightData.LightCounts.y = 0;
        lightData.EnvSkyColor   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // envIntensity = 1.0
        gl.UpdateLightingUBO(lightData);

        Leon::TestGPU::SetMat4(shader, "u_Model", glm::mat4(1.0f));
        Leon::TestGPU::SetMat3(shader, "u_NormalMatrix", glm::mat3(1.0f));
        shader->SetInt("u_UseIBL", 1); // Enable Real IBL
        shader->SetInt("u_UseShadows", 0);
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_DebugMode", 0);
        shader->SetFloat("u_AO", 1.0f);
        Leon::TestGPU::SetFloat3(shader, "u_EmissiveColor", glm::vec3(0.0f));
        Leon::TestGPU::SetFloat3(shader, "u_AlbedoColor", glm::vec3(1.0f));
        shader->SetFloat("u_Metallic", 0.0f);
        shader->SetFloat("u_Roughness", 0.5f);

        // 1. Create and Bind Constant White Irradiance Cubemap (L = PI = 3.14159)
        GLuint irradTex = 0;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &irradTex);
        glTextureStorage2D(irradTex, 1, GL_RGBA16F, 1, 1);
        std::vector<float> piFace(4, 3.14159265f);
        for (int f = 0; f < 6; ++f) {
            glTextureSubImage3D(irradTex, 0, 0, 0, f, 1, 1, 1, GL_RGBA, GL_FLOAT, piFace.data());
        }

        // 2. Create and Bind Constant Prefilter Cubemap (L = 1.0)
        GLuint prefTex = 0;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &prefTex);
        glTextureStorage2D(prefTex, 5, GL_RGBA16F, 16, 16);
        for (int m = 0; m < 5; ++m) {
            int s = 16 >> m;
            std::vector<float> oneFace(s * s * 4, 1.0f);
            for (int f = 0; f < 6; ++f) {
                glTextureSubImage3D(prefTex, m, 0, 0, f, s, s, 1, GL_RGBA, GL_FLOAT, oneFace.data());
            }
        }

        // 3. Create and Bind BRDF LUT (A = 0.8, B = 0.1)
        GLuint lutTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &lutTex);
        glTextureStorage2D(lutTex, 1, GL_RG16F, 1, 1);
        float lutData[2] = {0.8f, 0.1f};
        glTextureSubImage2D(lutTex, 0, 0, 0, 1, 1, GL_RG, GL_FLOAT, lutData);

        // Bind to respective texture units
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, lutTex);

        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradTex);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefTex);

        // Render IBL Pass
        gl.DrawQuad();
        glm::vec4 pixelIBL = gl.ReadPixel(0, 0);

        // Verify positive finite IBL ambient radiance
        CHECK(!std::isnan(pixelIBL.r));
        CHECK(!std::isnan(pixelIBL.g));
        CHECK(!std::isnan(pixelIBL.b));
        CHECK(pixelIBL.r > 0.5f);

        // 4. Pure Metallic IBL Test (kD == 0, diffuseIBL == 0 -> validates BRDF LUT)
        // specularIBL = prefilteredColor * (F_IBL * lut.x + lut.y) = 1.0 * (1.0 * 0.8 + 0.1) = 0.90
        shader->SetFloat("u_Metallic", 1.0f);
        gl.DrawQuad();
        glm::vec4 pixelMetalIBL = gl.ReadPixel(0, 0);

        CHECK(pixelMetalIBL.r == doctest::Approx(0.90f).epsilon(0.02f));
        CHECK(pixelMetalIBL.g == doctest::Approx(0.90f).epsilon(0.02f));
        CHECK(pixelMetalIBL.b == doctest::Approx(0.90f).epsilon(0.02f));

        // 5. Black HDR Test (Zero Irradiance -> Zero IBL)
        std::vector<float> zeroFace(4, 0.0f);
        for (int f = 0; f < 6; ++f) {
            glTextureSubImage3D(irradTex, 0, 0, 0, f, 1, 1, 1, GL_RGBA, GL_FLOAT, zeroFace.data());
            for (int m = 0; m < 5; ++m) {
                int s = 16 >> m;
                std::vector<float> zMFace(s * s * 4, 0.0f);
                glTextureSubImage3D(prefTex, m, 0, 0, f, s, s, 1, GL_RGBA, GL_FLOAT, zMFace.data());
            }
        }

        gl.DrawQuad();
        glm::vec4 pixelBlackIBL = gl.ReadPixel(0, 0);

        CHECK(pixelBlackIBL.r == doctest::Approx(0.0f).epsilon(1e-4f));
        CHECK(pixelBlackIBL.g == doctest::Approx(0.0f).epsilon(1e-4f));
        CHECK(pixelBlackIBL.b == doctest::Approx(0.0f).epsilon(1e-4f));

        // Cleanup
        glDeleteTextures(1, &irradTex);
        glDeleteTextures(1, &prefTex);
        glDeleteTextures(1, &lutTex);
    }
}
