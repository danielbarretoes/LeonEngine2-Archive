#include "Renderer/FParticleRenderer.hpp"
#include "Engine/Components.hpp"
#include "Engine/FParticleTypes.hpp"
#include "Engine/UWorld.hpp"
#include "RHI/FRenderCommand.hpp"
#include "RHI/FBuffer.hpp"
#include "Core/FFrameProfiler.hpp"

#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Leon {

    TRef<FShader> FParticleRenderer::Shader = nullptr;
    TRef<FVertexArray> FParticleRenderer::VertexArray = nullptr;
    TRef<FVertexBuffer> FParticleRenderer::VertexBuffer = nullptr;

    void FParticleRenderer::Init() {
        Shader = FShader::Create("Engine/Resources/Shaders/DebugLine.glsl");
        VertexArray = FVertexArray::Create();
        VertexBuffer = FVertexBuffer::Create(MaxVertices * sizeof(FVertex));
        VertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"}, {EShaderDataType::Float4, "aColor"}});
        VertexArray->AddVertexBuffer(VertexBuffer);
    }

    void FParticleRenderer::Shutdown() {
        Shader.reset();
        VertexBuffer.reset();
        VertexArray.reset();
    }

    void FParticleRenderer::Render(UWorld* InWorld, const FPerspectiveCamera& InCamera) {
        if (!InWorld || !Shader)
            return;

        std::vector<FVertex> verts;
        verts.reserve(512);

        const glm::vec3 camRight = InCamera.GetRightDirection();
        const glm::vec3 camUp = InCamera.GetUpDirection();
        const glm::vec3 camPos = InCamera.GetPosition();
        const float cullDistSq = kParticleCullDistance * kParticleCullDistance;

        auto pushQuad = [&](const glm::vec3& center, const glm::vec3& right, const glm::vec3& up, float halfW,
                            float halfH, const glm::vec4& color) {
            const glm::vec3 p0 = center - right * halfW - up * halfH;
            const glm::vec3 p1 = center + right * halfW - up * halfH;
            const glm::vec3 p2 = center + right * halfW + up * halfH;
            const glm::vec3 p3 = center - right * halfW + up * halfH;
            verts.push_back({p0, color});
            verts.push_back({p1, color});
            verts.push_back({p2, color});
            verts.push_back({p0, color});
            verts.push_back({p2, color});
            verts.push_back({p3, color});
        };

        auto& reg = InWorld->GetRegistry();
        int32_t count = 0;
        auto view = reg.view<FParticleRenderComponent>();
        for (auto entity : view) {
            const auto& render = view.get<FParticleRenderComponent>(entity);
            if (!render.bVisible)
                continue;
            for (const auto& p : render.Particles) {
                if (count >= kMaxRenderedParticles)
                    break;
                const glm::vec3 samplePos =
                    p.Kind == EParticleKind::Beam ? (p.Location + p.BeamEnd) * 0.5f : p.Location;
                const glm::vec3 toCam = samplePos - camPos;
                if (glm::dot(toCam, toCam) > cullDistSq)
                    continue;
                const float t = p.Lifetime > 1e-4f ? std::clamp(p.Age / p.Lifetime, 0.0f, 1.0f) : 1.0f;
                const glm::vec4 color = glm::mix(p.Color, p.ColorEnd, t);
                const float size = glm::mix(p.Size, p.SizeEnd, t);
                if (p.Kind == EParticleKind::Beam) {
                    glm::vec3 delta = p.BeamEnd - p.Location;
                    const float len = glm::length(delta);
                    if (len < 1e-4f)
                        continue;
                    glm::vec3 along = delta / len;
                    glm::vec3 side = glm::cross(along, camUp);
                    if (glm::length(side) < 1e-4f)
                        side = glm::cross(along, camRight);
                    side = glm::normalize(side);
                    const glm::vec3 mid = (p.Location + p.BeamEnd) * 0.5f;
                    pushQuad(mid, along, side, len * 0.5f, size, color);
                } else {
                    pushQuad(p.Location, camRight, camUp, size, size, color);
                }
                ++count;
                if (verts.size() + 6 >= MaxVertices)
                    break;
            }
            if (verts.size() + 6 >= MaxVertices)
                break;
        }

        FFrameProfiler::Working().ParticleCount = count;
        if (verts.empty())
            return;

        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::One);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(false);
        FRenderCommand::SetCulling(false);

        Shader->Bind();
        glm::mat4 vp = InCamera.GetViewProjectionMatrix();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(vp));
        Shader->SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));
        VertexBuffer->SetData(verts.data(), static_cast<unsigned int>(verts.size() * sizeof(FVertex)));
        VertexArray->Bind();
        FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(verts.size()));

        FRenderCommand::SetBlendState(false);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
        FRenderCommand::SetDepthMask(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);
    }

} // namespace Leon
