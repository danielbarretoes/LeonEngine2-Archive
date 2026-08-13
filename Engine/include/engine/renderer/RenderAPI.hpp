#pragma once

#include "engine/core/Base.hpp"

namespace Leon {

    class FVertexArray;

    enum class ERenderAPI { None = 0, OpenGL = 1 };

    using RenderAPIEnum = ERenderAPI;

    class IRenderAPI {
    public:
        using API = ERenderAPI;

        virtual ~IRenderAPI() = default;

        virtual void Init() = 0;
        virtual void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) = 0;
        virtual void SetClearColor(float InR, float InG, float InB, float InA) = 0;
        virtual void Clear() = 0;

        virtual void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) = 0;
        virtual void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) = 0;

        static ERenderAPI GetAPI() { return s_API; }
        static void SetAPI(ERenderAPI InAPI) { s_API = InAPI; }

        static TScope<IRenderAPI> Create();

    private:
        static ERenderAPI s_API;
    };

    using RenderAPI = IRenderAPI;

} // namespace Leon
