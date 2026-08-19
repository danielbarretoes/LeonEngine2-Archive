#pragma once

#include "Core/Base.hpp"
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
        DEPTH32F,
        DEPTH32F_SHADOW,
        DEPTH32F_ARRAY_SHADOW,

        // Defaults
        Depth = DEPTH24STENCIL8
    };

    struct FFramebufferTextureSpecification {
        FFramebufferTextureSpecification() = default;
        FFramebufferTextureSpecification(EFramebufferTextureFormat InFormat) : TextureFormat(InFormat) {}

        EFramebufferTextureFormat TextureFormat = EFramebufferTextureFormat::None;
    };

    struct FFramebufferAttachmentSpecification {
        FFramebufferAttachmentSpecification() = default;
        FFramebufferAttachmentSpecification(std::initializer_list<FFramebufferTextureSpecification> InAttachments)
            : Attachments(InAttachments) {}

        std::vector<FFramebufferTextureSpecification> Attachments;
    };

    struct FFramebufferSpecification {
        uint32_t Width = 0;
        uint32_t Height = 0;
        FFramebufferAttachmentSpecification Attachments;
        uint32_t Samples = 1;
        uint32_t ArrayLayers = 1;
        // Color attachment mip count (1 = single level). Use >1 for roughness-blurred planar reflections.
        uint32_t ColorMipLevels = 1;
        bool bSwapChainTarget = false;
        // Optional F1 HUD label. Identical names aggregate (e.g. all bloom mips as "Bloom").
        std::string DebugName;
    };

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
        virtual void AttachDepthTextureLayer(uint32_t InLayer) = 0;
        virtual void BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) = 0;
        // Builds mip chain for color attachments when ColorMipLevels > 1 (e.g. after planar pass).
        virtual void GenerateColorMipmaps() = 0;
        virtual const FFramebufferSpecification& GetSpecification() const = 0;

        static TRef<FFramebuffer> Create(const FFramebufferSpecification& InSpec);
    };

} // namespace Leon
