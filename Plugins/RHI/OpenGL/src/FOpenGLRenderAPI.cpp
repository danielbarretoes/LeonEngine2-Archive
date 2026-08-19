#include "FOpenGLRenderAPI.hpp"
#include "FOpenGLShader.hpp"
#include "RHI/FVertexArray.hpp"
#include "RHI/FRenderer.hpp"

#include <glad/glad.h>

namespace Leon {

    void FOpenGLRenderAPI::Init() {
        glEnable(GL_DEPTH_TEST);
        bDepthTestEnabled = true;

        glDepthFunc(GL_LESS);
        DepthFunc = EDepthFunc::Less;

        glDepthMask(GL_TRUE);
        bDepthMaskEnabled = true;

        glDisable(GL_BLEND);
        bBlendEnabled = false;
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        SrcBlend = EBlendFactor::SrcAlpha;
        DstBlend = EBlendFactor::OneMinusSrcAlpha;

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        bCullEnabled = true;
        CullMode = ECullMode::Back;

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
        if (bDepthTestEnabled == InEnabled)
            return;
        bDepthTestEnabled = InEnabled;
        if (InEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void FOpenGLRenderAPI::SetDepthMask(bool InEnabled) {
        if (bDepthMaskEnabled == InEnabled)
            return;
        bDepthMaskEnabled = InEnabled;
        glDepthMask(InEnabled ? GL_TRUE : GL_FALSE);
    }

    void FOpenGLRenderAPI::SetDepthFunc(EDepthFunc InFunc) {
        if (DepthFunc == InFunc)
            return;
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
        if (bCullEnabled == bEffectiveEnable && (!bEffectiveEnable || CullMode == InMode))
            return;
        bCullEnabled = bEffectiveEnable;
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
        if (bBlendEnabled == InEnabled)
            return;
        bBlendEnabled = InEnabled;
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

    void FOpenGLRenderAPI::SetClipDistance(bool InEnabled) {
        if (InEnabled)
            glEnable(GL_CLIP_DISTANCE0);
        else
            glDisable(GL_CLIP_DISTANCE0);
    }

    void FOpenGLRenderAPI::SetPolygonOffset(bool InEnabled, float InFactor, float InUnits) {
        if (InEnabled) {
            // FILL for meshes; LINE so wireframe debug colliders/traces can bias against coplanar surfaces.
            glEnable(GL_POLYGON_OFFSET_FILL);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(InFactor, InUnits);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(0.0f, 0.0f);
        }
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
        if (CurrentFBO == InRendererID)
            return;
        CurrentFBO = InRendererID;
        glBindFramebuffer(GL_FRAMEBUFFER, InRendererID);
        FRenderer::GetStatsMutable().FBOSwitches++;
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

    void FOpenGLRenderAPI::DrawIndexedInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                                unsigned int InInstanceCount) {
        unsigned int count = InIndexCount ? InIndexCount : InVertexArray->GetIndexBuffer()->GetCount();
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr, (GLsizei)InInstanceCount);
    }

    void FOpenGLRenderAPI::DrawIndexedOffsetInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                                      unsigned int InIndexOffset, unsigned int InInstanceCount) {
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)InIndexCount, GL_UNSIGNED_INT,
                                (const void*)(uintptr_t)(InIndexOffset * sizeof(uint32_t)),
                                (GLsizei)InInstanceCount);
    }

    void FOpenGLRenderAPI::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_LINES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::SetLineWidth(float InWidth) {
        glLineWidth(InWidth);
    }

    FOpenGLRenderAPI::~FOpenGLRenderAPI() {
        if (bGPUQueriesReady)
            glDeleteQueries(static_cast<GLsizei>(kGPUQueryFrames * kGPUQuerySlots), &GPUQueries[0][0]);
    }

    void FOpenGLRenderAPI::EnsureGPUQueries() {
        if (bGPUQueriesReady)
            return;
        glGenQueries(static_cast<GLsizei>(kGPUQueryFrames * kGPUQuerySlots), &GPUQueries[0][0]);
        bGPUQueriesReady = true;
    }

    void FOpenGLRenderAPI::BeginGPUTimeQuery(uint32_t InSlot) {
        EnsureGPUQueries();
        if (InSlot >= kGPUQuerySlots || GPUActiveSlot >= 0)
            return;
        GPUActiveSlot = static_cast<int>(InSlot);
        glBeginQuery(GL_TIME_ELAPSED, GPUQueries[GPUWriteFrame][InSlot]);
    }

    void FOpenGLRenderAPI::EndGPUTimeQuery(uint32_t InSlot) {
        if (GPUActiveSlot != static_cast<int>(InSlot))
            return;
        glEndQuery(GL_TIME_ELAPSED);
        GPUQueryIssued[GPUWriteFrame][InSlot] = true;
        GPUActiveSlot = -1;
    }

    void FOpenGLRenderAPI::ResolveGPUTimeQueries() {
        if (!bGPUQueriesReady)
            return;
        if (GPUActiveSlot >= 0) {
            glEndQuery(GL_TIME_ELAPSED);
            GPUQueryIssued[GPUWriteFrame][static_cast<uint32_t>(GPUActiveSlot)] = true;
            GPUActiveSlot = -1;
        }

        const int readFrame = (GPUWriteFrame + 1) % static_cast<int>(kGPUQueryFrames);
        for (uint32_t slot = 0; slot < kGPUQuerySlots; ++slot) {
            if (!GPUQueryIssued[readFrame][slot])
                continue;
            GLuint available = 0;
            glGetQueryObjectuiv(GPUQueries[readFrame][slot], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available)
                continue;
            GLuint64 ns = 0;
            glGetQueryObjectui64v(GPUQueries[readFrame][slot], GL_QUERY_RESULT, &ns);
            GPUResolvedMs[slot] = static_cast<float>(static_cast<double>(ns) / 1.0e6);
            GPUQueryIssued[readFrame][slot] = false;
        }

        GPUWriteFrame = (GPUWriteFrame + 1) % static_cast<int>(kGPUQueryFrames);
        for (uint32_t slot = 0; slot < kGPUQuerySlots; ++slot)
            GPUQueryIssued[GPUWriteFrame][slot] = false;
    }

    float FOpenGLRenderAPI::GetGPUTimeMs(uint32_t InSlot) const {
        if (InSlot >= kGPUQuerySlots)
            return 0.0f;
        return GPUResolvedMs[InSlot];
    }

    void FOpenGLRenderAPI::InvalidateShaderBindingCache() { FOpenGLShader::InvalidateBoundCache(); }

} // namespace Leon
