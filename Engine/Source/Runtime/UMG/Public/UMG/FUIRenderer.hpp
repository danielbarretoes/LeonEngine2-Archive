#pragma once

#include "Core/Base.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FTexture.hpp"
#include "RHI/FVertexArray.hpp"
#include "Engine/Components.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief 2D Orthographic Batch Renderer for UI Widgets, HUD, and On-Screen Debug Messages.
     */
    class FUIRenderer {
    public:
        struct FUIVertex {
            glm::vec2 Position;
            glm::vec2 TexCoord;
            glm::vec4 Color;
        };

        static void Init();
        static void Shutdown();

        /**
         * @brief Begins a 2D UI rendering frame with orthographic projection.
         */
        static void Begin(uint32_t InViewportWidth, uint32_t InViewportHeight);

        /**
         * @brief Ends and flushes all queued 2D UI draw commands.
         */
        static void End();

        /**
         * @brief Draws a solid or textured 2D quad in screen space pixels.
         */
        static void DrawQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InColor,
                             const glm::vec2& InUVMin = {0, 0}, const glm::vec2& InUVMax = {1, 1});

        /**
         * @brief Draws a textured 2D quad (flushes pending solid quads first).
         */
        static void DrawTexturedQuad(const glm::vec2& InMin, const glm::vec2& InMax, const TRef<FTexture2D>& InTexture,
                                     const glm::vec4& InTint = glm::vec4(1.0f), const glm::vec2& InUVMin = {0, 0},
                                     const glm::vec2& InUVMax = {1, 1});

        /**
         * @brief Draws a solid quad with an outline border.
         */
        static void DrawBorderQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InFillColor,
                                   const glm::vec4& InBorderColor, float InBorderWidth = 1.0f);

        /**
         * @brief Draws a 2D string in screen space pixels.
         */
        static void DrawString(float InX, float InY, const std::string& InText,
                               const glm::vec4& InColor = glm::vec4(1.0f), float InScale = 1.0f,
                               ETextAlignment InAlignment = ETextAlignment::Left);

        /**
         * @brief Measures the pixel bounding width and height of a string at a given scale.
         */
        static glm::vec2 MeasureString(const std::string& InText, float InScale = 1.0f);

    private:
        static void Flush(bool bUseTexture);
        static void FlushTextured(const TRef<FTexture2D>& InTexture, const std::vector<FUIVertex>& InVertices);

        static TRef<FShader> Shader;
        static TRef<FTexture2D> FontTexture;
        static TRef<FVertexArray> VertexArray;
        static TRef<FVertexBuffer> VertexBuffer;

        static std::vector<FUIVertex> QuadVertices;
        static std::vector<FUIVertex> TextVertices;

        static glm::mat4 OrthoMatrix;
        static uint32_t ViewportWidth;
        static uint32_t ViewportHeight;
        static bool bInitialized;

        static constexpr size_t MaxUIVertices = 16384;
    };

} // namespace Leon
