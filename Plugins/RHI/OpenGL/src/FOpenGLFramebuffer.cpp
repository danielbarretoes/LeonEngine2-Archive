#include "FOpenGLFramebuffer.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderer.hpp"
#include "RHI/FRenderCommand.hpp"

#include <algorithm>

namespace Leon {

    static const uint32_t MaxFramebufferSize = 8192;

    namespace Utils {

        static bool IsDepthFormat(EFramebufferTextureFormat InFormat) {
            switch (InFormat) {
            case EFramebufferTextureFormat::DEPTH24STENCIL8:
            case EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW:
            case EFramebufferTextureFormat::DEPTH32F:
            case EFramebufferTextureFormat::DEPTH32F_SHADOW:
            case EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW:
            case EFramebufferTextureFormat::DEPTH32F_CUBE_ARRAY:
                return true;
            default:
                return false;
            }
        }

        static GLenum LeonFBTextureFormatToGL(EFramebufferTextureFormat InFormat) {
            switch (InFormat) {
            case EFramebufferTextureFormat::RGBA8:
                return GL_RGBA8;
            case EFramebufferTextureFormat::RGBA16F:
                return GL_RGBA16F;
            case EFramebufferTextureFormat::RED_INTEGER:
                return GL_RED_INTEGER;
            default:
                return 0;
            }
        }

    } // namespace Utils

    FOpenGLFramebuffer::FOpenGLFramebuffer(const FFramebufferSpecification& InSpec) : Specification(InSpec) {
        for (auto& spec : Specification.Attachments.Attachments) {
            if (!Utils::IsDepthFormat(spec.TextureFormat))
                ColorAttachmentSpecs.emplace_back(spec);
            else
                DepthAttachmentSpec = spec;
        }

        Invalidate();
    }

    FOpenGLFramebuffer::~FOpenGLFramebuffer() {
        Cleanup();
    }

    void FOpenGLFramebuffer::Cleanup() {
        if (RendererID) {
            glDeleteFramebuffers(1, &RendererID);
            glDeleteTextures(static_cast<GLsizei>(ColorAttachments.size()), ColorAttachments.data());
            if (DepthAttachment) {
                glDeleteTextures(1, &DepthAttachment);
                DepthAttachment = 0;
            }

            ColorAttachments.clear();
            RendererID = 0;

            if (AllocatedBytes > 0) {
                const char* label = Specification.DebugName.empty() ? nullptr : Specification.DebugName.c_str();
                FRenderer::OnGPUFree(AllocatedBytes, EGPUMemoryCategory::Framebuffer, label);
                AllocatedBytes = 0;
            }
        }
    }

    void FOpenGLFramebuffer::Invalidate() {
        Cleanup();

        glCreateFramebuffers(1, &RendererID);

        size_t totalBytes = 0;

        // 1. Color Attachments (DSA)
        if (!ColorAttachmentSpecs.empty()) {
            ColorAttachments.resize(ColorAttachmentSpecs.size());
            for (size_t i = 0; i < ColorAttachments.size(); i++) {
                GLenum internalFormat = GL_RGBA8;
                size_t bpp = 4;
                switch (ColorAttachmentSpecs[i].TextureFormat) {
                case EFramebufferTextureFormat::RGBA8:
                    internalFormat = GL_RGBA8;
                    bpp = 4;
                    break;
                case EFramebufferTextureFormat::RGBA16F:
                    internalFormat = GL_RGBA16F;
                    bpp = 8;
                    break;
                case EFramebufferTextureFormat::RED_INTEGER:
                    internalFormat = GL_R32I;
                    bpp = 4;
                    break;
                default:
                    break;
                }

                const uint32_t mipLevels = std::max(1u, Specification.ColorMipLevels);

                glCreateTextures(GL_TEXTURE_2D, 1, &ColorAttachments[i]);
                glTextureStorage2D(ColorAttachments[i], static_cast<GLsizei>(mipLevels), internalFormat,
                                   Specification.Width, Specification.Height);

                const GLint minFilter = (mipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
                glTextureParameteri(ColorAttachments[i], GL_TEXTURE_MIN_FILTER, minFilter);
                glTextureParameteri(ColorAttachments[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(ColorAttachments[i], GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTextureParameteri(ColorAttachments[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(ColorAttachments[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                // Always attach mip 0 for rendering; higher levels filled via GenerateColorMipmaps().
                glNamedFramebufferTexture(RendererID, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                          ColorAttachments[i], 0);

                // Approximate full mip-chain VRAM (geometric series ≈ 4/3 of level-0).
                const size_t level0Bytes = static_cast<size_t>(Specification.Width) * Specification.Height * bpp;
                totalBytes += (mipLevels > 1) ? (level0Bytes * 4 / 3) : level0Bytes;
            }
        }

        // 2. Depth Attachment (DSA - 2D or 2D Array)
        if (DepthAttachmentSpec.TextureFormat != EFramebufferTextureFormat::None) {
            if (DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_CUBE_ARRAY) {
                uint32_t cubes = std::max(Specification.ArrayLayers, 1u);
                glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &DepthAttachment);
                glTextureStorage3D(DepthAttachment, 1, GL_DEPTH_COMPONENT32F, Specification.Width, Specification.Height,
                                   6 * cubes);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glNamedFramebufferTextureLayer(RendererID, GL_DEPTH_ATTACHMENT, DepthAttachment, 0, 0);
                totalBytes += Specification.Width * Specification.Height * 4 * 6 * cubes;
            } else if (DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW) {
                uint32_t layers = std::max(Specification.ArrayLayers, 1u);
                glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &DepthAttachment);
                glTextureStorage3D(DepthAttachment, 1, GL_DEPTH_COMPONENT32F, Specification.Width, Specification.Height,
                                   layers);

                glTextureParameteri(DepthAttachment, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

                float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTextureParameterfv(DepthAttachment, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach layer 0 by default
                glNamedFramebufferTextureLayer(RendererID, GL_DEPTH_ATTACHMENT, DepthAttachment, 0, 0);
                totalBytes += Specification.Width * Specification.Height * 4 * layers;
            } else {
                bool bIsShadow =
                    (DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW ||
                     DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_SHADOW);
                bool bIs32F = (DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F ||
                               DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_SHADOW);

                GLenum internalFormat = bIs32F ? GL_DEPTH_COMPONENT32F : GL_DEPTH24_STENCIL8;
                GLenum attachmentType = bIs32F ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT;

                glCreateTextures(GL_TEXTURE_2D, 1, &DepthAttachment);
                glTextureStorage2D(DepthAttachment, 1, internalFormat, Specification.Width, Specification.Height);

                // Depth is not interpolatable. LINEAR on the HDR depth (SSAO at half-res)
                // blends neighbouring window-Z values and paints a camera-facing floor band.
                glTextureParameteri(DepthAttachment, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(DepthAttachment, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                if (bIsShadow) {
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                    glTextureParameterfv(DepthAttachment, GL_TEXTURE_BORDER_COLOR, borderColor);
                } else {
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTextureParameteri(DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }

                glNamedFramebufferTexture(RendererID, attachmentType, DepthAttachment, 0);
                totalBytes += Specification.Width * Specification.Height * 4;
            }
        }

        // 3. Draw buffers setup (DSA)
        if (ColorAttachments.size() > 1) {
            LE_CORE_ASSERT(ColorAttachments.size() <= 4, "Only up to 4 color attachments are supported!");
            GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                 GL_COLOR_ATTACHMENT3};
            glNamedFramebufferDrawBuffers(RendererID, static_cast<GLsizei>(ColorAttachments.size()), buffers);
            glNamedFramebufferReadBuffer(RendererID, GL_COLOR_ATTACHMENT0);
        } else if (ColorAttachments.size() == 1) {
            glNamedFramebufferDrawBuffer(RendererID, GL_COLOR_ATTACHMENT0);
            glNamedFramebufferReadBuffer(RendererID, GL_COLOR_ATTACHMENT0);
        } else if (ColorAttachments.empty()) {
            glNamedFramebufferDrawBuffer(RendererID, GL_NONE);
            glNamedFramebufferReadBuffer(RendererID, GL_NONE);
        }

        GLenum status = glCheckNamedFramebufferStatus(RendererID, GL_FRAMEBUFFER);
        LE_CORE_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "FFramebuffer is incomplete!");

        AllocatedBytes = totalBytes;
        if (AllocatedBytes > 0) {
            const char* label = Specification.DebugName.empty() ? nullptr : Specification.DebugName.c_str();
            FRenderer::OnGPUAlloc(AllocatedBytes, EGPUMemoryCategory::Framebuffer, label);
        }
    }

    void FOpenGLFramebuffer::Bind() {
        FRenderCommand::BindFramebuffer(RendererID);
        FRenderCommand::SetViewport(0, 0, Specification.Width, Specification.Height);
    }

    void FOpenGLFramebuffer::Unbind() {
        FRenderCommand::BindFramebuffer(0);
    }

    void FOpenGLFramebuffer::Resize(uint32_t InWidth, uint32_t InHeight) {
        if (InWidth == 0 || InHeight == 0 || InWidth > MaxFramebufferSize || InHeight > MaxFramebufferSize) {
            LE_CORE_WARN("Attempted to resize framebuffer to ({0}, {1})", InWidth, InHeight);
            return;
        }

        Specification.Width = InWidth;
        Specification.Height = InHeight;
        Invalidate();
    }

    int FOpenGLFramebuffer::ReadPixel(uint32_t InAttachmentIndex, int InX, int InY) {
        LE_CORE_ASSERT(InAttachmentIndex < ColorAttachments.size(), "Attachment index out of bounds!");

        glNamedFramebufferReadBuffer(RendererID, GL_COLOR_ATTACHMENT0 + InAttachmentIndex);
        int pixelData = 0;
        glReadPixels(InX, InY, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
        return pixelData;
    }

    void FOpenGLFramebuffer::ClearAttachment(uint32_t InAttachmentIndex, int InValue) {
        LE_CORE_ASSERT(InAttachmentIndex < ColorAttachments.size(), "Attachment index out of bounds!");

        auto& spec = ColorAttachmentSpecs[InAttachmentIndex];
        glClearTexImage(ColorAttachments[InAttachmentIndex], 0, Utils::LeonFBTextureFormatToGL(spec.TextureFormat),
                        GL_INT, &InValue);
    }

    void FOpenGLFramebuffer::BindTexture(uint32_t InAttachmentIndex, uint32_t InSlot) const {
        LE_CORE_ASSERT(InAttachmentIndex < ColorAttachments.size(), "Attachment index out of bounds!");
        glBindTextureUnit(InSlot, ColorAttachments[InAttachmentIndex]);
        FRenderer::GetStatsMutable().TextureBinds++;
    }

    void FOpenGLFramebuffer::BindDepthTexture(uint32_t InSlot) const {
        LE_CORE_ASSERT(DepthAttachment != 0, "No depth attachment in framebuffer!");
        glBindTextureUnit(InSlot, DepthAttachment);
        FRenderer::GetStatsMutable().TextureBinds++;
    }

    void FOpenGLFramebuffer::AttachDepthTextureLayer(uint32_t InLayer) {
        LE_CORE_ASSERT(DepthAttachment != 0, "No depth attachment in framebuffer!");
        glNamedFramebufferTextureLayer(RendererID, GL_DEPTH_ATTACHMENT, DepthAttachment, 0,
                                       static_cast<GLint>(InLayer));
    }

    void FOpenGLFramebuffer::BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) {
        glBlitNamedFramebuffer(RendererID, 0, 0, 0, static_cast<GLint>(Specification.Width),
                               static_cast<GLint>(Specification.Height), 0, 0, static_cast<GLint>(InTargetWidth),
                               static_cast<GLint>(InTargetHeight), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    void FOpenGLFramebuffer::GenerateColorMipmaps() {
        if (Specification.ColorMipLevels <= 1)
            return;

        for (uint32_t colorTex : ColorAttachments) {
            if (colorTex != 0)
                glGenerateTextureMipmap(colorTex);
        }
    }

} // namespace Leon
