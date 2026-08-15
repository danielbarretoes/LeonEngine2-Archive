#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl UV Transformation") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU UV Tiling and Offset") {
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
        shader->SetInt("u_UseNormalMap", 0);
        shader->SetInt("u_UseAlbedoMap", 0);
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);

        // Mode 22: UV Debug Output (fract(uv))
        shader->SetInt("u_DebugMode", 22);

        // 1. Identity Tiling (1.0, 1.0) and Offset (0.0, 0.0)
        // Quad vertex coordinates: UV ranges [0, 1], center pixel is at (0.5, 0.5)
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f, 1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f, 0.0f));
        gl.DrawQuad();
        glm::vec4 centerUV = gl.ReadPixel();
        CHECK(centerUV.r == doctest::Approx(0.5f).epsilon(0.05f));
        CHECK(centerUV.g == doctest::Approx(0.5f).epsilon(0.05f));

        // 2. Custom Tiling (2.0, 3.0)
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(2.0f, 3.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f, 0.0f));
        gl.DrawQuad();
        glm::vec4 tiledUV = gl.ReadPixel();
        // At center (0.5, 0.5): u = 0.5 * 2.0 = 1.0 -> fract = 0.0; v = 0.5 * 3.0 = 1.5 -> fract = 0.5
        CHECK(std::abs(tiledUV.r) < 0.05f);
        CHECK(tiledUV.g == doctest::Approx(0.5f).epsilon(0.05f));

        // 3. Custom Offset (0.25, 0.10) with Tiling (1.0, 1.0)
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f, 1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.25f, 0.10f));
        gl.DrawQuad();
        glm::vec4 offsetUV = gl.ReadPixel();
        // At center: u = 0.5 + 0.25 = 0.75; v = 0.5 + 0.10 = 0.60
        CHECK(offsetUV.r == doctest::Approx(0.75f).epsilon(0.05f));
        CHECK(offsetUV.g == doctest::Approx(0.60f).epsilon(0.05f));
    }

}
