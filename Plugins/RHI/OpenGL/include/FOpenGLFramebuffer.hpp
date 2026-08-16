#pragma once

#include "RHI/FFramebuffer.hpp"
#include <glad/glad.h>
#include <vector>

namespace Leon {

    class FOpenGLFramebuffer : public FFramebuffer {
    public:
        FOpenGLFramebuffer(const FFramebufferSpecification& InSpec);
        ~FOpenGLFramebuffer() override;

        void Invalidate();

        void Bind() override;
        void Unbind() override;

        void Resize(uint32_t InWidth, uint32_t InHeight) override;
        int ReadPixel(uint32_t InAttachmentIndex, int InX, int InY) override;
        void ClearAttachment(uint32_t InAttachmentIndex, int InValue) override;

        uint32_t GetRendererID() const override { return RendererID; }

        uint32_t GetColorAttachmentRendererID(uint32_t InIndex = 0) const override {
            LE_CORE_ASSERT(InIndex < ColorAttachments.size(), "Attachment index out of bounds!");
            return ColorAttachments[InIndex];
        }

        uint32_t GetDepthAttachmentRendererID() const override { return DepthAttachment; }

        void BindTexture(uint32_t InAttachmentIndex = 0, uint32_t InSlot = 0) const override;
        void BindDepthTexture(uint32_t InSlot = 0) const override;
        void AttachDepthTextureLayer(uint32_t InLayer) override;

        void BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) override;
        void GenerateColorMipmaps() override;

        const FFramebufferSpecification& GetSpecification() const override { return Specification; }

    private:
        void Cleanup();

    private:
        uint32_t RendererID = 0;
        FFramebufferSpecification Specification;

        std::vector<FFramebufferTextureSpecification> ColorAttachmentSpecs;
        FFramebufferTextureSpecification DepthAttachmentSpec = EFramebufferTextureFormat::None;

        std::vector<uint32_t> ColorAttachments;
        uint32_t DepthAttachment = 0;

        size_t AllocatedBytes = 0;
    };

} // namespace Leon
