#pragma once

#include "core/Base.hpp"
#include <vector>

namespace Leon {

    enum class EFramebufferTextureFormat {
        None = 0,

        // Color Formats
        RGBA8,
        RGBA16F,
        RED_INTEGER,

        // Depth / Stencil Formats
        DEPTH24STENCIL8,
        DEPTH24STENCIL8_SHADOW,

        // Defaults
        Depth = DEPTH24STENCIL8
    };

    using FramebufferTextureFormat = EFramebufferTextureFormat;

    struct FFramebufferTextureSpecification {
        FFramebufferTextureSpecification() = default;
        FFramebufferTextureSpecification(EFramebufferTextureFormat InFormat) : TextureFormat(InFormat) {}

        EFramebufferTextureFormat TextureFormat = EFramebufferTextureFormat::None;
    };

    using FramebufferTextureSpecification = FFramebufferTextureSpecification;

    struct FFramebufferAttachmentSpecification {
        FFramebufferAttachmentSpecification() = default;
        FFramebufferAttachmentSpecification(std::initializer_list<FFramebufferTextureSpecification> InAttachments)
            : Attachments(InAttachments) {}

        std::vector<FFramebufferTextureSpecification> Attachments;
    };

    using FramebufferAttachmentSpecification = FFramebufferAttachmentSpecification;

    struct FFramebufferSpecification {
        uint32_t Width = 0;
        uint32_t Height = 0;
        FFramebufferAttachmentSpecification Attachments;
        uint32_t Samples = 1;
        bool bSwapChainTarget = false;
    };

    using FramebufferSpecification = FFramebufferSpecification;

    class FFramebuffer {
    public:
        virtual ~FFramebuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;

        virtual void Resize(uint32_t InWidth, uint32_t InHeight) = 0;
        virtual int ReadPixel(uint32_t InAttachmentIndex, int InX, int InY) = 0;
        virtual void ClearAttachment(uint32_t InAttachmentIndex, int InValue) = 0;

        virtual uint32_t GetRendererID() const = 0;
        virtual uint32_t GetColorAttachmentRendererID(uint32_t InIndex = 0) const = 0;
        virtual uint32_t GetDepthAttachmentRendererID() const = 0;
        virtual void BindTexture(uint32_t InAttachmentIndex = 0, uint32_t InSlot = 0) const = 0;
        virtual void BindDepthTexture(uint32_t InSlot = 0) const = 0;
        virtual void BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) = 0;
        virtual const FFramebufferSpecification& GetSpecification() const = 0;

        static TRef<FFramebuffer> Create(const FFramebufferSpecification& InSpec);
    };

    using Framebuffer = FFramebuffer;

} // namespace Leon
