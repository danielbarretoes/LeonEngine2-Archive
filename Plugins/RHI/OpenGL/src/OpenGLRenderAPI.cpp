#include "OpenGLRenderAPI.hpp"
#include "renderer/VertexArray.hpp"

#include <glad/glad.h>

namespace Leon {

    void FOpenGLRenderAPI::Init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
    }

    void FOpenGLRenderAPI::SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth,
                                       unsigned int InHeight) {
        glViewport((GLint)InX, (GLint)InY, (GLsizei)InWidth, (GLsizei)InHeight);
    }

    void FOpenGLRenderAPI::SetClearColor(float InR, float InG, float InB, float InA) {
        glClearColor(InR, InG, InB, InA);
    }

    void FOpenGLRenderAPI::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void FOpenGLRenderAPI::SetDepthTesting(bool InEnabled) {
        if (InEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void FOpenGLRenderAPI::SetDepthMask(bool InEnabled) {
        glDepthMask(InEnabled ? GL_TRUE : GL_FALSE);
    }

    void FOpenGLRenderAPI::SetDepthFunc(EDepthFunc InFunc) {
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
        if (InEnabled) {
            glEnable(GL_CULL_FACE);
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
            }
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    void FOpenGLRenderAPI::SetBlendState(bool InEnabled) {
        if (InEnabled) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
    }

    uint32_t FOpenGLRenderAPI::GetFramebufferBinding() {
        GLint fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        return static_cast<uint32_t>(fbo);
    }

    void FOpenGLRenderAPI::BindFramebuffer(uint32_t InRendererID) {
        glBindFramebuffer(GL_FRAMEBUFFER, InRendererID);
    }

    void FOpenGLRenderAPI::DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount) {
        unsigned int count = InIndexCount ? InIndexCount : InVertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
    }

    void FOpenGLRenderAPI::DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_LINES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::SetLineWidth(float InWidth) {
        glLineWidth(InWidth);
    }

} // namespace Leon
