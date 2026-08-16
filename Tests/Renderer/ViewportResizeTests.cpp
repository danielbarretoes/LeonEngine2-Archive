#include <doctest/doctest.h>
#include "GPU/HeadlessGLContext.hpp"
#include "RHI/FFramebuffer.hpp"

TEST_SUITE("Renderer pipeline - viewport / FBO resize") {

    TEST_CASE("HDR-style FBO resize matches requested extents") {
        auto& gl = Leon::TestGPU::FHeadlessGLContext::Get();
        if (!gl.IsValid())
            return;

        Leon::FFramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = {Leon::EFramebufferTextureFormat::RGBA16F, Leon::EFramebufferTextureFormat::DEPTH24STENCIL8};
        auto fb = Leon::FFramebuffer::Create(spec);
        REQUIRE(fb != nullptr);
        CHECK(fb->GetSpecification().Width == 1280);
        CHECK(fb->GetSpecification().Height == 720);

        const uint32_t sizes[][2] = {{1920, 1080}, {2560, 1440}, {800, 600}, {1280, 720}};
        for (auto size : sizes) {
            fb->Resize(size[0], size[1]);
            CHECK(fb->GetSpecification().Width == size[0]);
            CHECK(fb->GetSpecification().Height == size[1]);
        }
        fb->Resize(1920, 1080);
        fb->Resize(800, 600);
        CHECK(fb->GetSpecification().Width == 800);
        CHECK(fb->GetSpecification().Height == 600);
    }
}
