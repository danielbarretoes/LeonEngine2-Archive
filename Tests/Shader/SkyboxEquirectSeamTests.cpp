#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FFramebuffer.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FShader.hpp"

#include <algorithm>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

TEST_SUITE("Shader GPU - Skybox.glsl equirect wrap") {

    TEST_CASE("HDR skybox looking toward -X has no 1x1-mip vertical seam") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const uint32_t width = 64;
        const uint32_t height = 64;
        Leon::FFramebufferSpecification spec;
        spec.Width = width;
        spec.Height = height;
        spec.Attachments = {Leon::EFramebufferTextureFormat::RGBA16F, Leon::EFramebufferTextureFormat::DEPTH24STENCIL8};
        auto fbo = Leon::FFramebuffer::Create(spec);
        REQUIRE(fbo != nullptr);

        auto shader = Leon::FShader::Create("Engine/Resources/Shaders/Skybox.glsl");
        REQUIRE(shader != nullptr);
        auto cube = Leon::FMeshPrimitives::CreateCube(2.0f);
        REQUIRE(cube != nullptr);

        const int hdrW = 256;
        const int hdrH = 128;
        std::vector<float> hdr(static_cast<size_t>(hdrW) * hdrH * 4, 0.0f);
        for (int y = 0; y < hdrH; ++y) {
            for (int x = 0; x < hdrW; ++x) {
                const size_t i = (static_cast<size_t>(y) * hdrW + x) * 4;
                const bool bSun = x >= 112 && x < 144;
                hdr[i + 0] = bSun ? 48.0f : 0.05f;
                hdr[i + 1] = bSun ? 40.0f : 0.15f;
                hdr[i + 2] = bSun ? 24.0f : 0.80f;
                hdr[i + 3] = 1.0f;
            }
        }

        GLuint hdrTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
        const int levels = 9;
        glTextureStorage2D(hdrTex, levels, GL_RGBA32F, hdrW, hdrH);
        glTextureParameteri(hdrTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(hdrTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(hdrTex, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(hdrTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureSubImage2D(hdrTex, 0, 0, 0, hdrW, hdrH, GL_RGBA, GL_FLOAT, hdr.data());
        glGenerateTextureMipmap(hdrTex);

        glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(70.0f), 1.0f, 0.1f, 1000.0f);

        fbo->Bind();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->Bind();
        shader->SetMat4("u_View", glm::value_ptr(view));
        shader->SetMat4("u_Projection", glm::value_ptr(proj));
        shader->SetFloat("u_EnvironmentIntensity", 1.0f);
        shader->SetInt("u_UseHDREnvironmentMap", 1);
        shader->SetInt("u_HDREnvironmentMap", 0);
        glBindTextureUnit(0, hdrTex);

        cube->Bind();
        Leon::FRenderCommand::DrawIndexed(cube);

        std::vector<float> pixels(static_cast<size_t>(width) * height * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->GetRendererID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_FLOAT, pixels.data());
        fbo->Unbind();

        std::vector<float> colLum(width, 0.0f);
        for (uint32_t x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (uint32_t y = 0; y < height; ++y) {
                const size_t i = (static_cast<size_t>(y) * width + x) * 4;
                sum += 0.2126f * pixels[i] + 0.7152f * pixels[i + 1] + 0.0722f * pixels[i + 2];
            }
            colLum[x] = sum / static_cast<float>(height);
        }

        std::vector<float> sorted = colLum;
        std::sort(sorted.begin(), sorted.end());
        const float median = sorted[sorted.size() / 2];
        const float maxLum = *std::max_element(colLum.begin(), colLum.end());
        REQUIRE(median > 0.02f);
        CHECK(maxLum < median * 2.5f);

        glDeleteTextures(1, &hdrTex);
        Leon::FRenderCommand::SetDepthMask(true);
        Leon::FRenderCommand::SetDepthFunc(Leon::EDepthFunc::Less);
    }
}
