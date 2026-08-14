#include "OpenGLFramebuffer.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"

namespace Leon {

    static const uint32_t s_MaxFramebufferSize = 8192;

    namespace Utils {

        static bool IsDepthFormat(EFramebufferTextureFormat InFormat) {
            switch (InFormat) {
            case EFramebufferTextureFormat::DEPTH24STENCIL8:
            case EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW:
            case EFramebufferTextureFormat::DEPTH32F:
            case EFramebufferTextureFormat::DEPTH32F_SHADOW:
            case EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW:
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

    FOpenGLFramebuffer::FOpenGLFramebuffer(const FFramebufferSpecification& InSpec) : m_Specification(InSpec) {
        for (auto& spec : m_Specification.Attachments.Attachments) {
            if (!Utils::IsDepthFormat(spec.TextureFormat))
                m_ColorAttachmentSpecs.emplace_back(spec);
            else
                m_DepthAttachmentSpec = spec;
        }

        Invalidate();
    }

    FOpenGLFramebuffer::~FOpenGLFramebuffer() {
        Cleanup();
    }

    void FOpenGLFramebuffer::Cleanup() {
        if (m_RendererID) {
            glDeleteFramebuffers(1, &m_RendererID);
            glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
            if (m_DepthAttachment) {
                glDeleteTextures(1, &m_DepthAttachment);
                m_DepthAttachment = 0;
            }

            m_ColorAttachments.clear();
            m_RendererID = 0;

            if (m_AllocatedBytes > 0) {
                FRenderer::OnGPUFree(m_AllocatedBytes);
                m_AllocatedBytes = 0;
            }
        }
    }

    void FOpenGLFramebuffer::Invalidate() {
        Cleanup();

        glCreateFramebuffers(1, &m_RendererID);

        size_t totalBytes = 0;

        // 1. Color Attachments (DSA)
        if (!m_ColorAttachmentSpecs.empty()) {
            m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());
            for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
                GLenum internalFormat = GL_RGBA8;
                size_t bpp = 4;
                switch (m_ColorAttachmentSpecs[i].TextureFormat) {
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

                glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachments[i]);
                glTextureStorage2D(m_ColorAttachments[i], 1, internalFormat, m_Specification.Width, m_Specification.Height);

                glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glNamedFramebufferTexture(m_RendererID, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), m_ColorAttachments[i], 0);
                totalBytes += m_Specification.Width * m_Specification.Height * bpp;
            }
        }

        // 2. Depth Attachment (DSA - 2D or 2D Array)
        if (m_DepthAttachmentSpec.TextureFormat != EFramebufferTextureFormat::None) {
            if (m_DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_ARRAY_SHADOW) {
                uint32_t layers = std::max(m_Specification.ArrayLayers, 1u);
                glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DepthAttachment);
                glTextureStorage3D(m_DepthAttachment, 1, GL_DEPTH_COMPONENT32F, m_Specification.Width, m_Specification.Height, layers);

                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

                float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTextureParameterfv(m_DepthAttachment, GL_TEXTURE_BORDER_COLOR, borderColor);

                // Attach layer 0 by default
                glNamedFramebufferTextureLayer(m_RendererID, GL_DEPTH_ATTACHMENT, m_DepthAttachment, 0, 0);
                totalBytes += m_Specification.Width * m_Specification.Height * 4 * layers;
            } else {
                bool bIsShadow = (m_DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH24STENCIL8_SHADOW ||
                                  m_DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_SHADOW);
                bool bIs32F = (m_DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F ||
                               m_DepthAttachmentSpec.TextureFormat == EFramebufferTextureFormat::DEPTH32F_SHADOW);

                GLenum internalFormat = bIs32F ? GL_DEPTH_COMPONENT32F : GL_DEPTH24_STENCIL8;
                GLenum attachmentType = bIs32F ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT;

                glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
                glTextureStorage2D(m_DepthAttachment, 1, internalFormat, m_Specification.Width, m_Specification.Height);

                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(m_DepthAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                if (bIsShadow) {
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                    glTextureParameterfv(m_DepthAttachment, GL_TEXTURE_BORDER_COLOR, borderColor);
                } else {
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTextureParameteri(m_DepthAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }

                glNamedFramebufferTexture(m_RendererID, attachmentType, m_DepthAttachment, 0);
                totalBytes += m_Specification.Width * m_Specification.Height * 4;
            }
        }

        // 3. Draw buffers setup (DSA)
        if (m_ColorAttachments.size() > 1) {
            LE_CORE_ASSERT(m_ColorAttachments.size() <= 4, "Only up to 4 color attachments are supported!");
            GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
            glNamedFramebufferDrawBuffers(m_RendererID, static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
        } else if (m_ColorAttachments.empty()) {
            glNamedFramebufferDrawBuffer(m_RendererID, GL_NONE);
            glNamedFramebufferReadBuffer(m_RendererID, GL_NONE);
        }

        GLenum status = glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER);
        LE_CORE_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");

        m_AllocatedBytes = totalBytes;
        if (m_AllocatedBytes > 0) {
            FRenderer::OnGPUAlloc(m_AllocatedBytes);
        }
    }

    void FOpenGLFramebuffer::Bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glViewport(0, 0, m_Specification.Width, m_Specification.Height);
    }

    void FOpenGLFramebuffer::Unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FOpenGLFramebuffer::Resize(uint32_t InWidth, uint32_t InHeight) {
        if (InWidth == 0 || InHeight == 0 || InWidth > s_MaxFramebufferSize || InHeight > s_MaxFramebufferSize) {
            LE_CORE_WARN("Attempted to resize framebuffer to ({0}, {1})", InWidth, InHeight);
            return;
        }

        m_Specification.Width = InWidth;
        m_Specification.Height = InHeight;
        Invalidate();
    }

    int FOpenGLFramebuffer::ReadPixel(uint32_t InAttachmentIndex, int InX, int InY) {
        LE_CORE_ASSERT(InAttachmentIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");

        glNamedFramebufferReadBuffer(m_RendererID, GL_COLOR_ATTACHMENT0 + InAttachmentIndex);
        int pixelData = 0;
        glReadPixels(InX, InY, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
        return pixelData;
    }

    void FOpenGLFramebuffer::ClearAttachment(uint32_t InAttachmentIndex, int InValue) {
        LE_CORE_ASSERT(InAttachmentIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");

        auto& spec = m_ColorAttachmentSpecs[InAttachmentIndex];
        glClearTexImage(m_ColorAttachments[InAttachmentIndex], 0, Utils::LeonFBTextureFormatToGL(spec.TextureFormat),
                        GL_INT, &InValue);
    }

    void FOpenGLFramebuffer::BindTexture(uint32_t InAttachmentIndex, uint32_t InSlot) const {
        LE_CORE_ASSERT(InAttachmentIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");
        glBindTextureUnit(InSlot, m_ColorAttachments[InAttachmentIndex]);
    }

    void FOpenGLFramebuffer::BindDepthTexture(uint32_t InSlot) const {
        LE_CORE_ASSERT(m_DepthAttachment != 0, "No depth attachment in framebuffer!");
        glBindTextureUnit(InSlot, m_DepthAttachment);
    }

    void FOpenGLFramebuffer::AttachDepthTextureLayer(uint32_t InLayer) {
        LE_CORE_ASSERT(m_DepthAttachment != 0, "No depth attachment in framebuffer!");
        glNamedFramebufferTextureLayer(m_RendererID, GL_DEPTH_ATTACHMENT, m_DepthAttachment, 0, static_cast<GLint>(InLayer));
    }

    void FOpenGLFramebuffer::BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) {
        glBlitNamedFramebuffer(m_RendererID, 0,
                               0, 0, static_cast<GLint>(m_Specification.Width), static_cast<GLint>(m_Specification.Height),
                               0, 0, static_cast<GLint>(InTargetWidth), static_cast<GLint>(InTargetHeight),
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

} // namespace Leon
