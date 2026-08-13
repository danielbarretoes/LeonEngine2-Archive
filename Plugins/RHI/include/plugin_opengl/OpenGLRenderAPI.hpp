#pragma once

#include "engine/renderer/RenderAPI.hpp"

namespace Leon {

    class FOpenGLRenderAPI : public IRenderAPI {
    public:
        void Init() override;
        void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) override;
        void SetClearColor(float InR, float InG, float InB, float InA) override;
        void Clear() override;

        void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) override;
        void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) override;
    };

    using OpenGLRenderAPI = FOpenGLRenderAPI;

} // namespace Leon
