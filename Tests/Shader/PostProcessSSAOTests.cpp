#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FFramebuffer.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FShader.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

namespace {

    void UploadHemisphereKernel(const Leon::TRef<Leon::FShader>& InShader) {
        const glm::vec3 samples[16] = {
            {0.12f, 0.04f, 0.22f},   {-0.18f, 0.09f, 0.31f}, {0.05f, -0.21f, 0.28f}, {-0.08f, -0.11f, 0.42f},
            {0.27f, 0.15f, 0.36f},   {-0.31f, 0.02f, 0.48f}, {0.14f, 0.33f, 0.41f},  {-0.22f, -0.29f, 0.55f},
            {0.41f, -0.07f, 0.52f},  {-0.37f, 0.24f, 0.61f}, {0.09f, -0.38f, 0.58f}, {0.33f, 0.31f, 0.66f},
            {-0.44f, -0.18f, 0.71f}, {0.19f, 0.46f, 0.74f},  {-0.28f, 0.39f, 0.80f}, {0.07f, -0.47f, 0.88f},
        };
        for (int i = 0; i < 16; ++i) {
            glm::vec3 s = glm::normalize(samples[i]);
            InShader->SetFloat3("u_Samples[" + std::to_string(i) + "]", s.x, s.y, s.z);
        }
    }

} // namespace

TEST_SUITE("Shader GPU - SSAO") {

    TEST_CASE("SSAO leaves a perspective floor unoccluded") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        const uint32_t width = 64;
        const uint32_t height = 64;

        Leon::FFramebufferSpecification sceneSpec;
        sceneSpec.Width = width;
        sceneSpec.Height = height;
        sceneSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA16F, Leon::EFramebufferTextureFormat::DEPTH32F};
        auto sceneFBO = Leon::FFramebuffer::Create(sceneSpec);
        REQUIRE(sceneFBO != nullptr);

        auto depthShader = Leon::FShader::Create("Engine/Resources/Shaders/ShadowDepth.glsl");
        REQUIRE(depthShader != nullptr);
        auto plane = Leon::FMeshPrimitives::CreatePlane(80.0f, 80.0f, 8, 8);
        REQUIRE(plane != nullptr);

        glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f, 4.0f, 8.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(70.0f), 1.0f, 0.1f, 100.0f);
        glm::mat4 vp = proj * view;

        sceneFBO->Bind();
        Leon::FRenderCommand::SetViewport(0, 0, width, height);
        Leon::FRenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        Leon::FRenderCommand::SetDepthTesting(true);
        Leon::FRenderCommand::SetDepthMask(true);
        Leon::FRenderCommand::SetCulling(false);
        Leon::FRenderCommand::SetBlendState(false);
        Leon::FRenderCommand::Clear();
        depthShader->Bind();
        depthShader->SetMat4("u_LightSpaceMatrix", glm::value_ptr(vp));
        depthShader->SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));
        plane->Bind();
        Leon::FRenderCommand::DrawIndexed(plane);
        sceneFBO->Unbind();

        Leon::FFramebufferSpecification aoSpec;
        aoSpec.Width = width;
        aoSpec.Height = height;
        aoSpec.Attachments = {Leon::EFramebufferTextureFormat::RGBA8};
        auto aoFBO = Leon::FFramebuffer::Create(aoSpec);
        REQUIRE(aoFBO != nullptr);

        auto ssao = Leon::FShader::Create("Engine/Resources/Shaders/SSAO.glsl");
        REQUIRE(ssao != nullptr);

        aoFBO->Bind();
        Leon::FRenderCommand::SetViewport(0, 0, width, height);
        Leon::FRenderCommand::SetDepthTesting(false);
        Leon::FRenderCommand::SetDepthMask(false);
        Leon::FRenderCommand::SetClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        Leon::FRenderCommand::Clear();
        ssao->Bind();
        sceneFBO->BindDepthTexture(0);
        ssao->SetInt("u_DepthTexture", 0);
        ssao->SetMat4("u_Projection", glm::value_ptr(proj));
        ssao->SetMat4("u_InverseProjection", glm::value_ptr(glm::inverse(proj)));
        ssao->SetFloat("u_Radius", 0.45f);
        ssao->SetFloat("u_Bias", 0.025f);
        ssao->SetInt("u_KernelSize", 16);
        UploadHemisphereKernel(ssao);
        gl.DrawQuad();

        std::vector<float> pixels(width * height * 4, 0.0f);
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_FLOAT, pixels.data());
        aoFBO->Unbind();

        float minAO = 1.0f;
        float sumAO = 0.0f;
        int count = 0;
        for (uint32_t y = height / 4; y < (3 * height) / 4; ++y) {
            for (uint32_t x = width / 4; x < (3 * width) / 4; ++x) {
                float ao = pixels[(y * width + x) * 4];
                minAO = std::min(minAO, ao);
                sumAO += ao;
                ++count;
            }
        }
        REQUIRE(count > 0);
        CHECK(sumAO / static_cast<float>(count) > 0.92f);
        CHECK(minAO > 0.80f);
    }

    TEST_CASE("SSAO composite keeps bright specular when AO is dark") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        REQUIRE(gl.IsValid());

        auto shader = Leon::FShader::Create("Engine/Resources/Shaders/SSAOComposite.glsl");
        REQUIRE(shader != nullptr);
        shader->Bind();

        gl.BindFramebuffer(1, 1);

        GLuint hdrTex = 0;
        GLuint aoTex = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &hdrTex);
        glCreateTextures(GL_TEXTURE_2D, 1, &aoTex);
        glTextureStorage2D(hdrTex, 1, GL_RGBA16F, 1, 1);
        glTextureStorage2D(aoTex, 1, GL_RGBA8, 1, 1);
        float hdr[4] = {3.0f, 3.0f, 3.0f, 1.0f};
        unsigned char ao[4] = {25, 25, 25, 255};
        glTextureSubImage2D(hdrTex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, hdr);
        glTextureSubImage2D(aoTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, ao);
        glBindTextureUnit(0, hdrTex);
        glBindTextureUnit(1, aoTex);
        shader->SetInt("u_HDRSceneTexture", 0);
        shader->SetInt("u_SSAOTexture", 1);
        shader->SetFloat("u_Intensity", 1.0f);
        shader->SetInt("u_DebugAO", 0);
        gl.DrawQuad();
        glm::vec4 pixel = gl.ReadPixel(0, 0);
        CHECK(pixel.r > 2.4f);
        CHECK(pixel.g > 2.4f);
        CHECK(pixel.b > 2.4f);

        glDeleteTextures(1, &hdrTex);
        glDeleteTextures(1, &aoTex);
    }
}
