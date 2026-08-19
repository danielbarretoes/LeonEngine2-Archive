#pragma once

#include "Core/Base.hpp"
#include "Renderer/FLight.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FVertexArray.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <utility>
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
        static void EndScene(bool bDepthTest = true);
        static void Flush(bool bDepthTest = true);

        // Basic Line & Shape Primitives
        static void DrawLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor = glm::vec4(1.0f));
        static void DrawWireSphere(const glm::vec3& InCenter, float InRadius,
                                   const glm::vec4& InColor = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                                   unsigned int InSegments = 24);
        static void DrawWireFrustum(const glm::mat4& InViewProjection,
                                    const glm::vec4& InColor = glm::vec4(0.95f, 0.55f, 0.15f, 1.0f));
        static void DrawWireCone(const glm::vec3& InApex, const glm::vec3& InDirection, float InRange, float InAngleDeg,
                                 const glm::vec4& InColor = glm::vec4(0.2f, 0.8f, 1.0f, 1.0f),
                                 unsigned int InSegments = 24);
        static void DrawArrow(const glm::vec3& InStart, const glm::vec3& InEnd,
                              const glm::vec4& InColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), float InHeadSize = 0.35f);

        static void DrawDebugLine(const glm::vec3& InP0, const glm::vec3& InP1,
                                  const glm::vec4& InColor = glm::vec4(1.0f)) {
            DrawLine(InP0, InP1, InColor);
        }
        static void DrawDebugSphere(const glm::vec3& InCenter, float InRadius,
                                    const glm::vec4& InColor = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                                    unsigned int InSegments = 24) {
            DrawWireSphere(InCenter, InRadius, InColor, InSegments);
        }
        static void DrawDebugBox(const glm::vec3& InCenter, const glm::vec3& InExtent,
                                 const glm::vec4& InColor = glm::vec4(0.2f, 1.0f, 0.3f, 1.0f));
        /**
         * InHalfHeight = Unreal half of total capsule height (hemispheres included).
         * Cylinder half drawn = HalfHeight − Radius; tip-to-tip = 2 * HalfHeight.
         */
        static void DrawDebugCapsule(const glm::vec3& InCenter, float InRadius, float InHalfHeight,
                                     const glm::vec4& InColor = glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
        static void DrawDebugPoint(const glm::vec3& InPoint, float InSize = 0.06f,
                                   const glm::vec4& InColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
        static void DrawDebugArrow(const glm::vec3& InStart, const glm::vec3& InEnd,
                                   const glm::vec4& InColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
                                   float InHeadSize = 0.35f) {
            DrawArrow(InStart, InEnd, InColor, InHeadSize);
        }

        static void SetTraceCaptureEnabled(bool bEnabled) { bTraceCapture = bEnabled; }
        static bool IsTraceCaptureEnabled() { return bTraceCapture; }
        static void RecordLineTrace(const glm::vec3& InStart, const glm::vec3& InEnd, bool bHit,
                                    const glm::vec3& InHitLocation, const glm::vec3& InHitNormal,
                                    uint8_t InChannel = 0);
        static void DrawDebugLineTrace(const glm::vec3& InStart, const glm::vec3& InEnd, bool bHit,
                                       const glm::vec3& InHitLocation, const glm::vec3& InHitNormal,
                                       uint8_t InChannel = 0) {
            RecordLineTrace(InStart, InEnd, bHit, InHitLocation, InHitNormal, InChannel);
        }
        static void QueueLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor);
        static void DrawQueuedTraces();
        static void ClearQueuedTraces();

        struct FLastTrace {
            glm::vec3 Start{0.0f};
            glm::vec3 End{0.0f};
            glm::vec3 Hit{0.0f};
            glm::vec3 Normal{0.0f, 1.0f, 0.0f};
            uint8_t Channel = 0;
            bool bHit = false;
            bool bValid = false;
        };
        static const FLastTrace& GetLastTrace() { return LastTrace; }

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

        static TRef<FShader> Shader;
        static TRef<FVertexArray> VertexArray;
        static TRef<FVertexBuffer> VertexBuffer;
        static std::vector<FDebugVertex> LineVertices;
        static glm::mat4 ViewProjection;
        static constexpr size_t MaxLineVertices = 65536;
        static bool bTraceCapture;
        static bool bInScene;
        static bool bSceneDepthTest;
        struct FQueuedTrace {
            glm::vec3 Start{0.0f};
            glm::vec3 End{0.0f};
            glm::vec3 Hit{0.0f};
            glm::vec3 Normal{0.0f, 1.0f, 0.0f};
            uint8_t Channel = 0;
            bool bHit = false;
        };
        static std::vector<FQueuedTrace> QueuedTraces;
        static std::vector<std::pair<glm::vec3, glm::vec3>> QueuedExtraLines;
        static std::vector<glm::vec4> QueuedExtraColors;
        static FLastTrace LastTrace;
    };

} // namespace Leon
