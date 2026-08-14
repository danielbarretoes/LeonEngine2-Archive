#pragma once

#include "core/Base.hpp"
#include "renderer/Light.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/Shader.hpp"
#include "renderer/VertexArray.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    /**
     * @brief Real-time 3D line, wireframe, and lighting debug gizmo renderer.
     */
    class FDebugRenderer {
    public:
        static void Init();
        static void Shutdown();

        static void BeginScene(const FPerspectiveCamera& InCamera);
        static void EndScene();
        static void Flush();

        // Basic Line & Shape Primitives
        static void DrawLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor = glm::vec4(1.0f));
        static void DrawWireSphere(const glm::vec3& InCenter, float InRadius,
                                   const glm::vec4& InColor = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                                   unsigned int InSegments = 24);
        static void DrawWireCone(const glm::vec3& InApex, const glm::vec3& InDirection, float InRange, float InAngleDeg,
                                 const glm::vec4& InColor = glm::vec4(0.2f, 0.8f, 1.0f, 1.0f),
                                 unsigned int InSegments = 24);
        static void DrawArrow(const glm::vec3& InStart, const glm::vec3& InEnd,
                              const glm::vec4& InColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), float InHeadSize = 0.35f);

        // Light Gizmo Helpers
        static void DrawPointLightGizmo(const FPointLight& InLight);
        static void DrawSpotLightGizmo(const FSpotLight& InLight);
        static void DrawDirectionalLightGizmo(const FDirectionalLight& InLight,
                                              const glm::vec3& InSceneCenter = glm::vec3(0.0f, 2.0f, 0.0f),
                                              float InLength = 2.5f);

    private:
        struct FDebugVertex {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        static TRef<FShader> s_Shader;
        static TRef<FVertexArray> s_VertexArray;
        static TRef<FVertexBuffer> s_VertexBuffer;
        static std::vector<FDebugVertex> s_LineVertices;
        static glm::mat4 s_ViewProjection;
        static constexpr size_t MaxLineVertices = 65536;
    };

} // namespace Leon
