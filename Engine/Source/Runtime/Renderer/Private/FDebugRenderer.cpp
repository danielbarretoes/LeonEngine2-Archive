#include "Renderer/FDebugRenderer.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderCommand.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    TRef<FShader> FDebugRenderer::Shader = nullptr;
    TRef<FVertexArray> FDebugRenderer::VertexArray = nullptr;
    TRef<FVertexBuffer> FDebugRenderer::VertexBuffer = nullptr;
    std::vector<FDebugRenderer::FDebugVertex> FDebugRenderer::LineVertices;
    glm::mat4 FDebugRenderer::ViewProjection = glm::mat4(1.0f);
    bool FDebugRenderer::bTraceCapture = false;
    bool FDebugRenderer::bInScene = false;
    bool FDebugRenderer::bSceneDepthTest = true;
    std::vector<FDebugRenderer::FQueuedTrace> FDebugRenderer::QueuedTraces;
    std::vector<std::pair<glm::vec3, glm::vec3>> FDebugRenderer::QueuedExtraLines;
    std::vector<glm::vec4> FDebugRenderer::QueuedExtraColors;
    FDebugRenderer::FLastTrace FDebugRenderer::LastTrace{};

    void FDebugRenderer::Init() {
        LE_CORE_INFO("Initializing DebugRenderer Subsystem...");

        Shader = FShader::Create("Engine/Assets/Shaders/DebugLine.glsl");

        VertexArray = FVertexArray::Create();
        VertexBuffer = FVertexBuffer::Create(MaxLineVertices * sizeof(FDebugVertex));
        VertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"}, {EShaderDataType::Float4, "aColor"}});
        VertexArray->AddVertexBuffer(VertexBuffer);

        LineVertices.reserve(4096);
    }

    void FDebugRenderer::Shutdown() {
        Shader.reset();
        VertexBuffer.reset();
        VertexArray.reset();
        LineVertices.clear();
    }

    void FDebugRenderer::BeginScene(const FPerspectiveCamera& InCamera) {
        // Keep lines queued during Tick/OnRender; only EndScene/Flush clears the buffer.
        ViewProjection = InCamera.GetViewProjectionMatrix();
        bInScene = true;
        bSceneDepthTest = true;
    }

    void FDebugRenderer::EndScene(bool bDepthTest) {
        bSceneDepthTest = bDepthTest;
        Flush(bDepthTest);
        bInScene = false;
    }

    void FDebugRenderer::Flush(bool bDepthTest) {
        if (LineVertices.empty() || !Shader)
            return;

        // Depth-test against the scene so meshes occlude traces/colliders; do not write depth.
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
        FRenderCommand::SetDepthTesting(bDepthTest);
        FRenderCommand::SetDepthMask(false);
        if (bDepthTest) {
            FRenderCommand::SetDepthFunc(EDepthFunc::LessEqual);
            FRenderCommand::SetPolygonOffset(true, -1.0f, -2.0f);
        }
        FRenderCommand::SetCulling(false);

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(ViewProjection));
        Shader->SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));

        VertexBuffer->SetData(LineVertices.data(),
                              static_cast<unsigned int>(LineVertices.size() * sizeof(FDebugVertex)));

        VertexArray->Bind();
        FRenderCommand::SetLineWidth(2.5f);
        FRenderCommand::DrawLines(VertexArray, static_cast<unsigned int>(LineVertices.size()));

        FRenderCommand::SetPolygonOffset(false);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetDepthFunc(EDepthFunc::Less);
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetLineWidth(1.0f);

        LineVertices.clear();
    }

    void FDebugRenderer::DrawLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor) {
        if (LineVertices.size() + 2 >= MaxLineVertices) {
            // Never GPU-flush outside an active debug scene (wrong FBO / no scene depth).
            if (bInScene)
                Flush(bSceneDepthTest);
            else
                LineVertices.clear();
        }

        LineVertices.push_back({InP0, InColor});
        LineVertices.push_back({InP1, InColor});
    }

    void FDebugRenderer::DrawDebugBox(const glm::vec3& InCenter, const glm::vec3& InExtent, const glm::vec4& InColor) {
        glm::vec3 minB = InCenter - InExtent;
        glm::vec3 maxB = InCenter + InExtent;
        glm::vec3 c[8] = {
            {minB.x, minB.y, minB.z}, {maxB.x, minB.y, minB.z}, {maxB.x, maxB.y, minB.z}, {minB.x, maxB.y, minB.z},
            {minB.x, minB.y, maxB.z}, {maxB.x, minB.y, maxB.z}, {maxB.x, maxB.y, maxB.z}, {minB.x, maxB.y, maxB.z},
        };
        DrawLine(c[0], c[1], InColor);
        DrawLine(c[1], c[2], InColor);
        DrawLine(c[2], c[3], InColor);
        DrawLine(c[3], c[0], InColor);
        DrawLine(c[4], c[5], InColor);
        DrawLine(c[5], c[6], InColor);
        DrawLine(c[6], c[7], InColor);
        DrawLine(c[7], c[4], InColor);
        DrawLine(c[0], c[4], InColor);
        DrawLine(c[1], c[5], InColor);
        DrawLine(c[2], c[6], InColor);
        DrawLine(c[3], c[7], InColor);
    }

    void FDebugRenderer::DrawDebugCapsule(const glm::vec3& InCenter, float InRadius, float InHalfHeight,
                                          const glm::vec4& InColor) {
        // UE half-height includes hemispheres → cylinder extent = HalfHeight − Radius.
        const float cyl = std::max(InHalfHeight - InRadius, 0.0f);
        glm::vec3 top = InCenter + glm::vec3(0.0f, cyl, 0.0f);
        glm::vec3 bot = InCenter + glm::vec3(0.0f, -cyl, 0.0f);
        DrawWireSphere(top, InRadius, InColor, 16);
        DrawWireSphere(bot, InRadius, InColor, 16);
        DrawLine(top + glm::vec3(InRadius, 0, 0), bot + glm::vec3(InRadius, 0, 0), InColor);
        DrawLine(top + glm::vec3(-InRadius, 0, 0), bot + glm::vec3(-InRadius, 0, 0), InColor);
        DrawLine(top + glm::vec3(0, 0, InRadius), bot + glm::vec3(0, 0, InRadius), InColor);
        DrawLine(top + glm::vec3(0, 0, -InRadius), bot + glm::vec3(0, 0, -InRadius), InColor);
    }

    void FDebugRenderer::DrawDebugPoint(const glm::vec3& InPoint, float InSize, const glm::vec4& InColor) {
        DrawLine(InPoint + glm::vec3(InSize, 0, 0), InPoint - glm::vec3(InSize, 0, 0), InColor);
        DrawLine(InPoint + glm::vec3(0, InSize, 0), InPoint - glm::vec3(0, InSize, 0), InColor);
        DrawLine(InPoint + glm::vec3(0, 0, InSize), InPoint - glm::vec3(0, 0, InSize), InColor);
    }

    void FDebugRenderer::DrawWireSphere(const glm::vec3& InCenter, float InRadius, const glm::vec4& InColor,
                                        unsigned int InSegments) {
        constexpr float PI = 3.14159265358979323846f;
        float step = (PI * 2.0f) / static_cast<float>(InSegments);

        // XY, XZ, YZ orthogonal circles
        for (unsigned int i = 0; i < InSegments; ++i) {
            float a0 = static_cast<float>(i) * step;
            float a1 = static_cast<float>(i + 1) * step;

            float c0 = std::cos(a0) * InRadius;
            float s0 = std::sin(a0) * InRadius;
            float c1 = std::cos(a1) * InRadius;
            float s1 = std::sin(a1) * InRadius;

            // XY Plane
            DrawLine(InCenter + glm::vec3(c0, s0, 0.0f), InCenter + glm::vec3(c1, s1, 0.0f), InColor);
            // XZ Plane
            DrawLine(InCenter + glm::vec3(c0, 0.0f, s0), InCenter + glm::vec3(c1, 0.0f, s1), InColor);
            // YZ Plane
            DrawLine(InCenter + glm::vec3(0.0f, c0, s0), InCenter + glm::vec3(0.0f, c1, s1), InColor);
        }
    }

    void FDebugRenderer::DrawWireCone(const glm::vec3& InApex, const glm::vec3& InDirection, float InRange,
                                      float InAngleDeg, const glm::vec4& InColor, unsigned int InSegments) {
        constexpr float PI = 3.14159265358979323846f;
        glm::vec3 dir = glm::normalize(InDirection);

        // Build orthogonal basis (U, V) perpendicular to dir
        glm::vec3 up = (std::abs(dir.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 u = glm::normalize(glm::cross(dir, up));
        glm::vec3 v = glm::cross(u, dir);

        float radius = InRange * std::tan(glm::radians(InAngleDeg));
        glm::vec3 baseCenter = InApex + dir * InRange;

        float step = (PI * 2.0f) / static_cast<float>(InSegments);

        // Base Circle
        for (unsigned int i = 0; i < InSegments; ++i) {
            float a0 = static_cast<float>(i) * step;
            float a1 = static_cast<float>(i + 1) * step;

            glm::vec3 p0 = baseCenter + (u * std::cos(a0) + v * std::sin(a0)) * radius;
            glm::vec3 p1 = baseCenter + (u * std::cos(a1) + v * std::sin(a1)) * radius;

            DrawLine(p0, p1, InColor);
        }

        // 4 Lateral Lines from Apex to Base
        DrawLine(InApex, baseCenter + u * radius, InColor);
        DrawLine(InApex, baseCenter - u * radius, InColor);
        DrawLine(InApex, baseCenter + v * radius, InColor);
        DrawLine(InApex, baseCenter - v * radius, InColor);
    }

    void FDebugRenderer::DrawArrow(const glm::vec3& InStart, const glm::vec3& InEnd, const glm::vec4& InColor,
                                   float InHeadSize) {
        DrawLine(InStart, InEnd, InColor);

        glm::vec3 dir = InEnd - InStart;
        float len = glm::length(dir);
        if (len < 0.001f)
            return;

        dir = glm::normalize(dir);
        glm::vec3 up = (std::abs(dir.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 u = glm::normalize(glm::cross(dir, up)) * InHeadSize;
        glm::vec3 v = glm::cross(u, dir);

        glm::vec3 headBase = InEnd - dir * (InHeadSize * 1.5f);
        DrawLine(InEnd, headBase + u, InColor);
        DrawLine(InEnd, headBase - u, InColor);
        DrawLine(InEnd, headBase + v, InColor);
        DrawLine(InEnd, headBase - v, InColor);
    }

    void FDebugRenderer::DrawPointLightGizmo(const FPointLight& InLight) {
        // PBR model: radius is explicit, no more Phong attenuation formula
        float radius = glm::clamp(InLight.Radius, 1.0f, 100.0f);

        glm::vec4 color(InLight.Color, 0.85f);

        // Center cross marker
        float crossSize = 0.2f;
        DrawLine(InLight.Position - glm::vec3(crossSize, 0, 0), InLight.Position + glm::vec3(crossSize, 0, 0),
                 glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        DrawLine(InLight.Position - glm::vec3(0, crossSize, 0), InLight.Position + glm::vec3(0, crossSize, 0),
                 glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        DrawLine(InLight.Position - glm::vec3(0, 0, crossSize), InLight.Position + glm::vec3(0, 0, crossSize),
                 glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // Outer Attenuation Sphere
        DrawWireSphere(InLight.Position, radius, color, 32);
    }

    void FDebugRenderer::DrawSpotLightGizmo(const FSpotLight& InLight) {
        float range = 4.5f;

        glm::vec4 innerColor(InLight.Color, 1.0f);
        glm::vec4 outerColor(InLight.Color * 0.6f, 0.5f);

        // Center ray
        DrawLine(InLight.Position, InLight.Position + glm::normalize(InLight.Direction) * range,
                 glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));

        // Inner Cone (Solid spotlight beam)
        DrawWireCone(InLight.Position, InLight.Direction, range, InLight.CutOff, innerColor, 24);

        // Outer Cone (Penumbra region)
        DrawWireCone(InLight.Position, InLight.Direction, range, InLight.OuterCutOff, outerColor, 24);
    }

    void FDebugRenderer::DrawDirectionalLightGizmo(const FDirectionalLight& InLight, const glm::vec3& InSceneCenter,
                                                   float InLength) {
        glm::vec4 sunColor(InLight.Color, 0.9f);
        glm::vec3 dir = glm::normalize(InLight.Direction);

        // Grid of 4 parallel sunlight rays pointing along InLight.Direction
        float offset = 1.2f;
        glm::vec3 up = (std::abs(dir.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        glm::vec3 normal = glm::cross(right, dir);

        for (int x = -1; x <= 1; x += 2) {
            for (int y = -1; y <= 1; y += 2) {
                glm::vec3 rayStart = InSceneCenter + right * (x * offset) + normal * (y * offset) - dir * InLength;
                glm::vec3 rayEnd = rayStart + dir * (InLength * 1.5f);
                DrawArrow(rayStart, rayEnd, sunColor, 0.3f);
            }
        }
    }

    void FDebugRenderer::RecordLineTrace(const glm::vec3& InStart, const glm::vec3& InEnd, bool bHit,
                                         const glm::vec3& InHitLocation, const glm::vec3& InHitNormal,
                                         uint8_t InChannel) {
        LastTrace.Start = InStart;
        LastTrace.End = InEnd;
        LastTrace.Hit = InHitLocation;
        LastTrace.Normal = InHitNormal;
        LastTrace.Channel = InChannel;
        LastTrace.bHit = bHit;
        LastTrace.bValid = true;
        if (!bTraceCapture)
            return;
        if (QueuedTraces.size() > 256)
            return;
        QueuedTraces.push_back({InStart, InEnd, InHitLocation, InHitNormal, InChannel, bHit});
    }

    void FDebugRenderer::QueueLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor) {
        if (!bTraceCapture)
            return;
        if (QueuedExtraLines.size() > 512)
            return;
        QueuedExtraLines.push_back({InP0, InP1});
        QueuedExtraColors.push_back(InColor);
    }

    void FDebugRenderer::DrawQueuedTraces() {
        const glm::vec4 missColor{0.2f, 0.85f, 1.0f, 0.9f};
        const glm::vec4 hitColor{1.0f, 0.25f, 0.15f, 0.95f};
        for (const auto& t : QueuedTraces) {
            const glm::vec4 color = t.bHit ? hitColor : missColor;
            DrawLine(t.Start, t.bHit ? t.Hit : t.End, color);
            if (t.bHit) {
                DrawWireSphere(t.Hit, 0.08f, hitColor, 12);
                DrawLine(t.Hit, t.Hit + t.Normal * 0.35f, glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
            }
        }
        for (size_t i = 0; i < QueuedExtraLines.size(); ++i) {
            const glm::vec4 c = i < QueuedExtraColors.size() ? QueuedExtraColors[i] : glm::vec4(1.0f);
            DrawLine(QueuedExtraLines[i].first, QueuedExtraLines[i].second, c);
        }
    }

    void FDebugRenderer::ClearQueuedTraces() {
        QueuedTraces.clear();
        QueuedExtraLines.clear();
        QueuedExtraColors.clear();
    }

} // namespace Leon
