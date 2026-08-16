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
        ViewProjection = InCamera.GetViewProjectionMatrix();
        LineVertices.clear();
    }

    void FDebugRenderer::EndScene() {
        Flush();
    }

    void FDebugRenderer::Flush() {
        if (LineVertices.empty() || !Shader)
            return;

        // Configure render states for debug wireframe rendering
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(ViewProjection));
        Shader->SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));

        VertexBuffer->SetData(LineVertices.data(),
                                static_cast<unsigned int>(LineVertices.size() * sizeof(FDebugVertex)));

        VertexArray->Bind();
        FRenderCommand::SetLineWidth(2.0f);
        FRenderCommand::DrawLines(VertexArray, static_cast<unsigned int>(LineVertices.size()));

        // Restore standard 3D depth testing defaults
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);

        LineVertices.clear();
    }

    void FDebugRenderer::DrawLine(const glm::vec3& InP0, const glm::vec3& InP1, const glm::vec4& InColor) {
        if (LineVertices.size() + 2 >= MaxLineVertices)
            Flush();

        LineVertices.push_back({InP0, InColor});
        LineVertices.push_back({InP1, InColor});
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

} // namespace Leon
