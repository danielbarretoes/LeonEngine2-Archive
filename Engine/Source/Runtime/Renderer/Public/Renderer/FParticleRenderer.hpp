#pragma once

#include "Core/Base.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"

#include <glm/glm.hpp>

namespace Leon {

    class UWorld;

    /**
     * Camera-facing additive quads and thin beams for UParticleComponent instances.
     */
    class FParticleRenderer {
    public:
        static void Init();
        static void Shutdown();
        static void Render(UWorld* InWorld, const FPerspectiveCamera& InCamera);

    private:
        struct FVertex {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        static TRef<FShader> Shader;
        static TRef<FVertexArray> VertexArray;
        static TRef<FVertexBuffer> VertexBuffer;
        static constexpr size_t MaxVertices = 16384;
    };

} // namespace Leon
