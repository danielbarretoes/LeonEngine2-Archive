#pragma once

#include "Core/Base.hpp"
#include "Core/FPlatformMemory.hpp"
#include "Core/FTimestep.hpp"
#include "Renderer/FRenderStats.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FTexture.hpp"
#include "RHI/FVertexArray.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Real-time 2D Performance & Diagnostics HUD Overlay.
     */
    class FDebugOverlay {
    public:
        static void Init();
        static void Shutdown();

        /**
         * @brief Renders the HUD Overlay on top of the current frame.
         * @param InViewportWidth Current window width.
         * @param InViewportHeight Current window height.
         * @param InDeltaTime Frame delta time.
         * @param InRenderStats Statistics gathered during this frame.
         * @param InbShowGizmos Current F2 Gizmo toggle state.
         */
        static void Render(unsigned int InViewportWidth, unsigned int InViewportHeight, FTimestep InDeltaTime,
                           const FRenderStats& InRenderStats, bool InbShowGizmos);

    private:
        struct FOverlayVertex {
            glm::vec2 Position;
            glm::vec2 TexCoord;
            glm::vec4 Color;
        };

        static void DrawQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InColor,
                             const glm::vec2& InUVMin = {0, 0}, const glm::vec2& InUVMax = {1, 1});
        static void DrawString(float InX, float InY, const std::string& InText, const glm::vec4& InColor = {1, 1, 1, 1},
                               float InScale = 1.0f);
        static void Flush(const glm::mat4& InOrthoMatrix);

        static TRef<FShader> Shader;
        static TRef<FTexture2D> FontTexture;
        static TRef<FVertexArray> VertexArray;
        static TRef<FVertexBuffer> VertexBuffer;
        static std::vector<FOverlayVertex> Vertices;

        static float SmoothedFPS;
        static float SmoothedFrameTimeMs;
        static FGPUInfo CachedGPUInfo;
        static bool bHasGPUInfo;

        static constexpr size_t MaxOverlayVertices = 8192;
    };

} // namespace Leon
