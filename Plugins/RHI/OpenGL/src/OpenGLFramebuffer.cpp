#include "OpenGLFramebuffer.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"

namespace Leon {

    static const uint32_t s_MaxFramebufferSize = 8192;

    namespace Utils {

        static GLenum TextureTarget(bool InMultisampled) {
            return InMultisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        }

        static void CreateTextures(bool InMultisampled, uint32_t* OutTextures, uint32_t InCount) {
            glGenTextures(InCount, OutTextures);
        }

        static void BindTexture(bool InMultisampled, uint32_t InRendererID) {
            glBindTexture(TextureTarget(InMultisampled), InRendererID);
        }

        static void AttachColorTexture(uint32_t InRendererID, int InSamples, GLenum InInternalFormat, GLenum InFormat,
                                       uint32_t InWidth, uint32_t InHeight, int InIndex) {
            bool isMultisampled = InSamples > 1;
            if (isMultisampled) {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, InSamples, InInternalFormat, InWidth, InHeight,
                                        GL_FALSE);
            } else {
                GLenum dataType = (InInternalFormat == GL_RGBA16F) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
                glTexImage2D(GL_TEXTURE_2D, 0, InInternalFormat, InWidth, InHeight, 0, InFormat, dataType,
                             nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + InIndex, TextureTarget(isMultisampled),
                                   InRendererID, 0);
        }

        static void AttachDepthTexture(uint32_t InRendererID, int InSamples, GLenum InFormat, GLenum InAttachmentType,
                                       uint32_t InWidth, uint32_t InHeight) {
            bool isMultisampled = InSamples > 1;
            if (isMultisampled) {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, InSamples, InFormat, InWidth, InHeight, GL_FALSE);
            } else {
                glTexImage2D(GL_TEXTURE_2D, 0, InFormat, InWidth, InHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                             nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, InAttachmentType, TextureTarget(isMultisampled), InRendererID, 0);
        }

        static bool IsDepthFormat(EFramebufferTextureFormat InFormat) {
            switch (InFormat) {
            case EFramebufferTextureFormat::DEPTH24STENCIL8:
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

        static size_t GetBytesPerPixel(EFramebufferTextureFormat InFormat) {
            switch (InFormat) {
            case EFramebufferTextureFormat::RGBA8:
                return 4;
            case EFramebufferTextureFormat::RGBA16F:
                return 8;
            case EFramebufferTextureFormat::RED_INTEGER:
                return 4;
            case EFramebufferTextureFormat::DEPTH24STENCIL8:
                return 4;
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
            glDeleteTextures(1, &m_DepthAttachment);

            m_ColorAttachments.clear();
            m_DepthAttachment = 0;
            m_RendererID = 0;

            if (m_AllocatedBytes > 0) {
                FRenderer::OnGPUFree(m_AllocatedBytes);
                m_AllocatedBytes = 0;
            }
        }
    }

    void FOpenGLFramebuffer::Invalidate() {
        Cleanup();

        glGenFramebuffers(1, &m_RendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

        bool isMultisampled = m_Specification.Samples > 1;
        size_t totalBytes = 0;

        // Attachments
        if (!m_ColorAttachmentSpecs.empty()) {
            m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());
            Utils::CreateTextures(isMultisampled, m_ColorAttachments.data(),
                                  static_cast<uint32_t>(m_ColorAttachments.size()));

            for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
                Utils::BindTexture(isMultisampled, m_ColorAttachments[i]);
                switch (m_ColorAttachmentSpecs[i].TextureFormat) {
                case EFramebufferTextureFormat::RGBA8:
                    Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA,
                                              m_Specification.Width, m_Specification.Height, static_cast<int>(i));
                    totalBytes += m_Specification.Width * m_Specification.Height * 4 * m_Specification.Samples;
                    break;
                case EFramebufferTextureFormat::RGBA16F:
                    Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA16F, GL_RGBA,
                                              m_Specification.Width, m_Specification.Height, static_cast<int>(i));
                    totalBytes += m_Specification.Width * m_Specification.Height * 8 * m_Specification.Samples;
                    break;
                case EFramebufferTextureFormat::RED_INTEGER:
                    Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER,
                                              m_Specification.Width, m_Specification.Height, static_cast<int>(i));
                    totalBytes += m_Specification.Width * m_Specification.Height * 4 * m_Specification.Samples;
                    break;
                default:
                    break;
                }
            }
        }

        if (m_DepthAttachmentSpec.TextureFormat != EFramebufferTextureFormat::None) {
            Utils::CreateTextures(isMultisampled, &m_DepthAttachment, 1);
            Utils::BindTexture(isMultisampled, m_DepthAttachment);
            switch (m_DepthAttachmentSpec.TextureFormat) {
            case EFramebufferTextureFormat::DEPTH24STENCIL8:
                Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8,
                                          GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
                totalBytes += m_Specification.Width * m_Specification.Height * 4 * m_Specification.Samples;
                break;
            default:
                break;
            }
        }

        if (m_ColorAttachments.size() > 1) {
            LE_CORE_ASSERT(m_ColorAttachments.size() <= 4, "Only up to 4 color attachments are supported!");
            GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                 GL_COLOR_ATTACHMENT3};
            glDrawBuffers(static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
        } else if (m_ColorAttachments.empty()) {
            // Only depth-pass
            glDrawBuffer(GL_NONE);
        }

        LE_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                       "Framebuffer is incomplete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

        glReadBuffer(GL_COLOR_ATTACHMENT0 + InAttachmentIndex);
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
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[InAttachmentIndex]);
    }

    void FOpenGLFramebuffer::BindDepthTexture(uint32_t InSlot) const {
        LE_CORE_ASSERT(m_DepthAttachment != 0, "No depth attachment in framebuffer!");
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
    }

    void FOpenGLFramebuffer::BlitToDefault(uint32_t InTargetWidth, uint32_t InTargetHeight) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, static_cast<GLint>(m_Specification.Width), static_cast<GLint>(m_Specification.Height),
                          0, 0, static_cast<GLint>(InTargetWidth), static_cast<GLint>(InTargetHeight),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

} // namespace Leon
