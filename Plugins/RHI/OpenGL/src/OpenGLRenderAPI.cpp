#include "plugin_opengl/OpenGLRenderAPI.hpp"
#include "engine/renderer/VertexArray.hpp"

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

    void FOpenGLRenderAPI::DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)InVertexCount);
    }

    void FOpenGLRenderAPI::DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount) {
        unsigned int count = InIndexCount ? InIndexCount : InVertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
    }

} // namespace Leon
