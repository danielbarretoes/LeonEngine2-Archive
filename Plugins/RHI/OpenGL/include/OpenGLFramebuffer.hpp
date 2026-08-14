#pragma once

#include "renderer/Framebuffer.hpp"
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

        uint32_t GetRendererID() const override { return m_RendererID; }

        uint32_t GetColorAttachmentRendererID(uint32_t InIndex = 0) const override {
            LE_CORE_ASSERT(InIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");
            return m_ColorAttachments[InIndex];
        }

        uint32_t GetDepthAttachmentRendererID() const override { return m_DepthAttachment; }

        void BindTexture(uint32_t InAttachmentIndex = 0, uint32_t InSlot = 0) const override;
        void BindDepthTexture(uint32_t InSlot = 0) const override;
        void AttachDepthTextureLayer(uint32_t InLayer) override;

        void BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) override;

        const FFramebufferSpecification& GetSpecification() const override { return m_Specification; }

    private:
        void Cleanup();

    private:
        uint32_t m_RendererID = 0;
        FFramebufferSpecification m_Specification;

        std::vector<FFramebufferTextureSpecification> m_ColorAttachmentSpecs;
        FFramebufferTextureSpecification m_DepthAttachmentSpec = EFramebufferTextureFormat::None;

        std::vector<uint32_t> m_ColorAttachments;
        uint32_t m_DepthAttachment = 0;

        size_t m_AllocatedBytes = 0;
    };

    using OpenGLFramebuffer = FOpenGLFramebuffer;

} // namespace Leon
