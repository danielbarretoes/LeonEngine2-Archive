#pragma once

#include "core/Base.hpp"
#include "renderer/Buffer.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"
#include "renderer/VertexArray.hpp"
#include "scene/Components.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief 3D In-World Text Rendering Subsystem (analogous to Unreal Engine UTextRenderComponent / ATextRenderActor).
     * Renders spatial, non-billboarded 3D text in world space with transforms, alignment, and TrueType fonts.
     */
    class FTextRenderer {
    public:
        struct FTextVertex {
            glm::vec3 Position;
            glm::vec2 TexCoord;
            glm::vec4 Color;
        };

        /**
         * @brief Initializes font atlas and shader pipelines.
         */
        static void Init();

        /**
         * @brief Releases font atlas and GPU buffers.
         */
        static void Shutdown();

        /**
         * @brief Prepares 3D text batching for the current camera view.
         * @param InCamera Camera providing ViewProjection matrices.
         */
        static void BeginScene(const FPerspectiveCamera& InCamera);

        /**
         * @brief Flushes all queued 3D text draw commands to the GPU.
         */
        static void EndScene();

        /**
         * @brief Submits a 3D in-world string to be rendered with a spatial transform.
         * @param InText String to render (supports multiline \n).
         * @param InTransform 3D Model transformation matrix (position, rotation, scale).
         * @param InColor Text RGBA tint.
         * @param InSize Height of text in world units.
         * @param InAlignment Horizontal text justification (Left, Center, Right).
         * @param InbDoubleSided Whether the text is visible and readable from both sides.
         */
        static void DrawString(const std::string& InText, const glm::mat4& InTransform,
                               const glm::vec4& InColor = glm::vec4(1.0f), float InSize = 1.0f,
                               ETextAlignment InAlignment = ETextAlignment::Center, bool InbDoubleSided = true);

        /**
         * @brief Measures the 2D bounding box width and height of a string at a given size.
         * @param InText String to measure.
         * @param InSize Font size multiplier.
         * @return glm::vec2 with width (x) and height (y) in local units.
         */
        static glm::vec2 MeasureString(const std::string& InText, float InSize = 1.0f);

    private:
        static void Flush();

        static TRef<FShader> s_Shader;
        static TRef<FTexture2D> s_FontTexture;
        static TRef<FVertexArray> s_VertexArray;
        static TRef<FVertexBuffer> s_VertexBuffer;

        static glm::mat4 s_ViewProjection;
        static std::vector<FTextVertex> s_Vertices;

        static constexpr size_t MaxVertices = 16384;
    };

} // namespace Leon
