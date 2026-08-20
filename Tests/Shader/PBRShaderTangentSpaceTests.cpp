#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include <filesystem>

TEST_SUITE("Shader GPU - PBR_Lit.glsl Tangent Space & TBN Orthonormality") {

    TEST_CASE("PBR_Lit.glsl Hardware GPU Tangent and Bitangent Reconstruction") {
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
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        // 1. Mode 20: Tangent Vector T (normalize(T) * 0.5 + 0.5)
        // Quad Tangent is (1, 0, 0) -> Mapped: (1.0, 0.5, 0.5)
        shader->SetInt("u_DebugMode", 20);
        gl.DrawQuad();
        glm::vec4 tangentColor = gl.ReadPixel();
        CHECK(tangentColor.r == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(tangentColor.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(tangentColor.b == doctest::Approx(0.5f).epsilon(0.02f));

        // 2. Mode 21: Bitangent Vector B (normalize(B) * 0.5 + 0.5)
        // Quad Bitangent is (0, 1, 0) -> Mapped: (0.5, 1.0, 0.5)
        shader->SetInt("u_DebugMode", 21);
        gl.DrawQuad();
        glm::vec4 bitangentColor = gl.ReadPixel();
        CHECK(bitangentColor.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(bitangentColor.g == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(bitangentColor.b == doctest::Approx(0.5f).epsilon(0.02f));

        // 3. Orthonormality check in reconstructed vectors
        glm::vec3 T = glm::vec3(tangentColor.r, tangentColor.g, tangentColor.b) * 2.0f - 1.0f;
        glm::vec3 B = glm::vec3(bitangentColor.r, bitangentColor.g, bitangentColor.b) * 2.0f - 1.0f;
        glm::vec3 N = glm::vec3(0.0f, 0.0f, 1.0f);

        CHECK(std::abs(glm::dot(T, N)) < 0.05f);
        CHECK(std::abs(glm::dot(B, N)) < 0.05f);
        CHECK(std::abs(glm::dot(T, B)) < 0.05f);
        CHECK(glm::length(T) == doctest::Approx(1.0f).epsilon(0.05f));
        CHECK(glm::length(B) == doctest::Approx(1.0f).epsilon(0.05f));
    }

    TEST_CASE("PBR_Lit.glsl Hardware GPU Gram-Schmidt Orthogonalization on Skewed Inputs") {
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
        shader->SetInt("u_UseMetallicMap", 0);
        shader->SetInt("u_UseRoughnessMap", 0);
        shader->SetInt("u_UseAOMap", 0);
        shader->SetInt("u_UseEmissiveMap", 0);
        shader->SetInt("u_UsePlanarReflection", 0);
        shader->SetInt("u_AlphaMode", 0);
        shader->SetFloat("u_AlphaCutoff", 0.5f);
        Leon::TestGPU::SetFloat2(shader, "u_UVTiling", glm::vec2(1.0f));
        Leon::TestGPU::SetFloat2(shader, "u_UVOffset", glm::vec2(0.0f));

        // Create custom mesh with SKEWED Tangent (1, 0, 1) and SKEWED Bitangent (1, 1, 0)
        // Normal is (0, 0, 1).
        // Gram-Schmidt MUST project T onto plane perp to N -> (1, 0, 0)
        // Gram-Schmidt MUST project B onto plane perp to N and T -> (0, 1, 0)
        std::vector<Leon::TestGPU::FTestVertex> skewedVertices = {
            {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f),
             glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)},
            {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f),
             glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)},
            {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f),
             glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)},
            {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f),
             glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)}};
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

        GLuint vao = 0, vbo = 0, ebo = 0;
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glCreateBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, skewedVertices.size() * sizeof(Leon::TestGPU::FTestVertex), skewedVertices.data(),
                     GL_STATIC_DRAW);

        glCreateBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, Position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, Normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, TexCoord));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, Tangent));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, Color));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(Leon::TestGPU::FTestVertex),
                              (void*)offsetof(Leon::TestGPU::FTestVertex, LightmapUV));

        // 1. Tangent test: Skewed (1, 0, 1) MUST be reconstructed to pure +X (1, 0, 0) -> (1.0, 0.5, 0.5)
        shader->SetInt("u_DebugMode", 20);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glm::vec4 tangPix = gl.ReadPixel();
        CHECK(tangPix.r == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(tangPix.g == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(tangPix.b == doctest::Approx(0.5f).epsilon(0.02f));

        // 2. Bitangent test: Skewed (1, 1, 0) MUST be reconstructed to pure +Y (0, 1, 0) -> (0.5, 1.0, 0.5)
        shader->SetInt("u_DebugMode", 21);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glm::vec4 bitangPix = gl.ReadPixel();
        CHECK(bitangPix.r == doctest::Approx(0.5f).epsilon(0.02f));
        CHECK(bitangPix.g == doctest::Approx(1.0f).epsilon(0.02f));
        CHECK(bitangPix.b == doctest::Approx(0.5f).epsilon(0.02f));

        glBindVertexArray(0);
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }
}
