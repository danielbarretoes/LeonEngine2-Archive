#include "FOpenGLRenderAPI.hpp"
#include "RHI/FVertexArray.hpp"

#include <glad/glad.h>

namespace Leon {

    void FOpenGLRenderAPI::Init() {
        // Default blend: SrcAlpha / OneMinusSrcAlpha
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        SrcBlend = EBlendFactor::SrcAlpha;
        DstBlend = EBlendFactor::OneMinusSrcAlpha;

        glEnable(GL_BLEND);
        BlendEnabled = true;

        glEnable(GL_DEPTH_TEST);
        DepthTestEnabled = true;

        glDepthFunc(GL_LESS);
        DepthFunc = EDepthFunc::Less;

        glDepthMask(GL_TRUE);
        DepthMaskEnabled = true;

        // Enable seamless cubemap filtering for artifacts-free IBL filtering
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    void FOpenGLRenderAPI::SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth,
                                       unsigned int InHeight) {
        if (ViewportX == InX && ViewportY == InY && ViewportW == InWidth && ViewportH == InHeight)
            return;

        ViewportX = InX;
        ViewportY = InY;
        ViewportW = InWidth;
        ViewportH = InHeight;
        glViewport((GLint)InX, (GLint)InY, (GLsizei)InWidth, (GLsizei)InHeight);
    }

    void FOpenGLRenderAPI::SetClearColor(float InR, float InG, float InB, float InA) {
        glClearColor(InR, InG, InB, InA);
    }

    void FOpenGLRenderAPI::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void FOpenGLRenderAPI::SetDepthTesting(bool InEnabled) {
        DepthTestEnabled = InEnabled;
        if (InEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void FOpenGLRenderAPI::SetDepthMask(bool InEnabled) {
        DepthMaskEnabled = InEnabled;
        glDepthMask(InEnabled ? GL_TRUE : GL_FALSE);
    }

    void FOpenGLRenderAPI::SetDepthFunc(EDepthFunc InFunc) {
        DepthFunc = InFunc;
        switch (InFunc) {
        case EDepthFunc::Less:
            glDepthFunc(GL_LESS);
            break;
        case EDepthFunc::LessEqual:
            glDepthFunc(GL_LEQUAL);
            break;
        case EDepthFunc::Equal:
            glDepthFunc(GL_EQUAL);
            break;
        case EDepthFunc::Always:
            glDepthFunc(GL_ALWAYS);
            break;
        }
    }

    void FOpenGLRenderAPI::SetCulling(bool InEnabled, ECullMode InMode) {
        bool bEffectiveEnable = InEnabled && (InMode != ECullMode::None);
        CullEnabled = bEffectiveEnable;
        if (bEffectiveEnable)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);

        if (bEffectiveEnable) {
            CullMode = InMode;
            switch (InMode) {
            case ECullMode::Back:
                glCullFace(GL_BACK);
                break;
            case ECullMode::Front:
                glCullFace(GL_FRONT);
                break;
            case ECullMode::FrontAndBack:
                glCullFace(GL_FRONT_AND_BACK);
                break;
            case ECullMode::None:
                break;
            }
        }
    }

    void FOpenGLRenderAPI::SetWireframe(bool InEnabled) {
        glPolygonMode(GL_FRONT_AND_BACK, InEnabled ? GL_LINE : GL_FILL);
    }

    void FOpenGLRenderAPI::SetBlendState(bool InEnabled) {
        BlendEnabled = InEnabled;
        if (InEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    static GLenum BlendFactorToGL(EBlendFactor InFactor) {
        switch (InFactor) {
        case EBlendFactor::Zero:
            return GL_ZERO;
        case EBlendFactor::One:
            return GL_ONE;
        case EBlendFactor::SrcColor:
            return GL_SRC_COLOR;
        case EBlendFactor::OneMinusSrcColor:
            return GL_ONE_MINUS_SRC_COLOR;
        case EBlendFactor::SrcAlpha:
            return GL_SRC_ALPHA;
        case EBlendFactor::OneMinusSrcAlpha:
            return GL_ONE_MINUS_SRC_ALPHA;
        case EBlendFactor::DstAlpha:
            return GL_DST_ALPHA;
        case EBlendFactor::OneMinusDstAlpha:
            return GL_ONE_MINUS_DST_ALPHA;
        case EBlendFactor::DstColor:
            return GL_DST_COLOR;
        case EBlendFactor::OneMinusDstColor:
            return GL_ONE_MINUS_DST_COLOR;
        }
        return GL_ONE;
    }

    void FOpenGLRenderAPI::SetBlendFunc(EBlendFactor InSrc, EBlendFactor InDst) {
        if (SrcBlend == InSrc && DstBlend == InDst)
            return;
        SrcBlend = InSrc;
        DstBlend = InDst;
        glBlendFunc(BlendFactorToGL(InSrc), BlendFactorToGL(InDst));
    }

    FGPUInfo FOpenGLRenderAPI::GetGPUInfo() {
        FGPUInfo info;
        const char* vendor = (const char*)glGetString(GL_VENDOR);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        const char* version = (const char*)glGetString(GL_VERSION);
        const char* slVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

        info.Vendor = vendor ? vendor : "Unknown Vendor";
        info.Renderer = renderer ? renderer : "Unknown GPU";
        info.Version = version ? version : "Unknown Version";
        info.ShadingLanguageVersion = slVersion ? slVersion : "Unknown GLSL";
        return info;
    }

    FGPUVRAMStats FOpenGLRenderAPI::GetGPUVRAMStats() {
        FGPUVRAMStats stats;
#ifndef GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#endif
#ifndef GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#endif
        GLint totalMemKb = 0;
        GLint curAvailKb = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &totalMemKb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &curAvailKb);

        if (totalMemKb > 0) {
            stats.TotalVRAMBytes = static_cast<size_t>(totalMemKb) * 1024;
            if (curAvailKb > 0 && totalMemKb >= curAvailKb) {
                stats.UsedVRAMBytes = static_cast<size_t>(totalMemKb - curAvailKb) * 1024;
            }
        }
        return stats;
    }

    uint32_t FOpenGLRenderAPI::GetFramebufferBinding() {
        return CurrentFBO;
    }

    void FOpenGLRenderAPI::BindFramebuffer(uint32_t InRendererID) {
        CurrentFBO = InRendererID;
        glBindFramebuffer(GL_FRAMEBUFFER, InRendererID);
    }

    void FOpenGLRenderAPI::DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount) {
        unsigned int count = InIndexCount ? InIndexCount : InVertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
    }

    void FOpenGLRenderAPI::DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                             unsigned int InIndexOffset) {
        glDrawElements(GL_TRIANGLES, (GLsizei)InIndexCount, GL_UNSIGNED_INT,
                       (const void*)(uintptr_t)(InIndexOffset * sizeof(uint32_t)));
    }

    void FOpenGLRenderAPI::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_LINES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::SetLineWidth(float InWidth) {
        glLineWidth(InWidth);
    }

} // namespace Leon
