#include "UMG/FUIRenderer.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderCommand.hpp"

#include <cmath>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_truetype.h>

namespace Leon {

    TRef<FShader> FUIRenderer::Shader = nullptr;
    TRef<FTexture2D> FUIRenderer::FontTexture = nullptr;
    TRef<FVertexArray> FUIRenderer::VertexArray = nullptr;
    TRef<FVertexBuffer> FUIRenderer::VertexBuffer = nullptr;

    std::vector<FUIRenderer::FUIVertex> FUIRenderer::QuadVertices;
    std::vector<FUIRenderer::FUIVertex> FUIRenderer::TextVertices;

    glm::mat4 FUIRenderer::OrthoMatrix = glm::mat4(1.0f);
    uint32_t FUIRenderer::ViewportWidth = 1280;
    uint32_t FUIRenderer::ViewportHeight = 720;
    bool FUIRenderer::bInitialized = false;

    static stbtt_bakedchar UIBakedChars[96];
    static bool bUIFontLoaded = false;
    static constexpr int kUIAtlasDim = 1024;
    static constexpr float kUIFontPixelHeight = 48.0f;
    static constexpr float kUIBaseline = 38.0f;
    static constexpr float kUILineHeight = 56.0f;

    void FUIRenderer::Init() {
        if (bInitialized)
            return;

        LE_CORE_INFO("Initializing UI Renderer Subsystem...");
        Shader = FShader::Create("Engine/Assets/Shaders/DebugFont.glsl");

        std::vector<unsigned char> tempBitmap(static_cast<size_t>(kUIAtlasDim) * kUIAtlasDim, 0);

        // Prefer Inter Bold for crisp HUD digits; fall back to Regular.
        const char* fontCandidates[] = {
            "Engine/Assets/Fonts/Inter-Bold.ttf",
            "Engine/Assets/Fonts/Inter-Regular.ttf",
        };
        for (const char* fontPath : fontCandidates) {
            std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
            if (!file.is_open())
                continue;
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<unsigned char> fontBuffer(static_cast<size_t>(size));
            if (!file.read(reinterpret_cast<char*>(fontBuffer.data()), size))
                continue;
            const int res = stbtt_BakeFontBitmap(fontBuffer.data(), 0, kUIFontPixelHeight, tempBitmap.data(),
                                                 kUIAtlasDim, kUIAtlasDim, 32, 96, UIBakedChars);
            if (res > 0) {
                bUIFontLoaded = true;
                LE_CORE_INFO("FUIRenderer: Baked Inter UI font from '{0}' at {1}px", fontPath,
                             static_cast<int>(kUIFontPixelHeight));
                break;
            }
        }
        if (!bUIFontLoaded)
            LE_CORE_WARN("FUIRenderer: Failed to load Inter font — HUD text may be blank");

        std::vector<unsigned char> rgbaAtlas(static_cast<size_t>(kUIAtlasDim) * kUIAtlasDim * 4, 0);
        for (size_t i = 0; i < tempBitmap.size(); ++i) {
            unsigned char a = tempBitmap[i];
            rgbaAtlas[i * 4 + 0] = a;
            rgbaAtlas[i * 4 + 1] = a;
            rgbaAtlas[i * 4 + 2] = a;
            rgbaAtlas[i * 4 + 3] = a;
        }

        FontTexture = FTexture2D::Create(kUIAtlasDim, kUIAtlasDim);
        FontTexture->SetData(rgbaAtlas.data(), static_cast<unsigned int>(rgbaAtlas.size()));

        VertexArray = FVertexArray::Create();
        VertexBuffer = FVertexBuffer::Create(MaxUIVertices * sizeof(FUIVertex));
        VertexBuffer->SetLayout({{EShaderDataType::Float2, "aPos"},
                                   {EShaderDataType::Float2, "aTexCoord"},
                                   {EShaderDataType::Float4, "aColor"}});
        VertexArray->AddVertexBuffer(VertexBuffer);

        QuadVertices.reserve(4096);
        TextVertices.reserve(4096);
        bInitialized = true;
    }

    void FUIRenderer::Shutdown() {
        Shader.reset();
        FontTexture.reset();
        VertexBuffer.reset();
        VertexArray.reset();
        QuadVertices.clear();
        TextVertices.clear();
        bInitialized = false;
    }

    void FUIRenderer::Begin(uint32_t InViewportWidth, uint32_t InViewportHeight) {
        if (!bInitialized) {
            Init();
        }

        // UI must never inherit scene wireframe (F3) polygon mode
        FRenderCommand::SetWireframe(false);

        ViewportWidth = InViewportWidth > 0 ? InViewportWidth : 1280;
        ViewportHeight = InViewportHeight > 0 ? InViewportHeight : 720;
        OrthoMatrix = glm::ortho(0.0f, static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight),
                                   0.0f, -1.0f, 1.0f);

        QuadVertices.clear();
        TextVertices.clear();
    }

    void FUIRenderer::DrawQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InColor,
                               const glm::vec2& InUVMin, const glm::vec2& InUVMax) {
        if (QuadVertices.size() + 6 >= MaxUIVertices)
            return;

        QuadVertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        QuadVertices.push_back({{InMax.x, InMin.y}, {InUVMax.x, InUVMin.y}, InColor});
        QuadVertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});

        QuadVertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        QuadVertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});
        QuadVertices.push_back({{InMin.x, InMax.y}, {InUVMin.x, InUVMax.y}, InColor});
    }

    void FUIRenderer::DrawTexturedQuad(const glm::vec2& InMin, const glm::vec2& InMax,
                                       const TRef<FTexture2D>& InTexture, const glm::vec4& InTint,
                                       const glm::vec2& InUVMin, const glm::vec2& InUVMax) {
        if (!InTexture) {
            DrawQuad(InMin, InMax, InTint, InUVMin, InUVMax);
            return;
        }
        if (!QuadVertices.empty()) {
            Flush(false);
            QuadVertices.clear();
        }

        std::vector<FUIVertex> verts;
        verts.reserve(6);
        verts.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InTint});
        verts.push_back({{InMax.x, InMin.y}, {InUVMax.x, InUVMin.y}, InTint});
        verts.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InTint});
        verts.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InTint});
        verts.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InTint});
        verts.push_back({{InMin.x, InMax.y}, {InUVMin.x, InUVMax.y}, InTint});
        FlushTextured(InTexture, verts);
    }

    void FUIRenderer::DrawBorderQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InFillColor,
                                     const glm::vec4& InBorderColor, float InBorderWidth) {
        // Draw background fill
        DrawQuad(InMin, InMax, InFillColor);

        // Draw top, bottom, left, right borders
        if (InBorderWidth > 0.0f && InBorderColor.a > 0.0f) {
            DrawQuad(InMin, {InMax.x, InMin.y + InBorderWidth}, InBorderColor); // Top
            DrawQuad({InMin.x, InMax.y - InBorderWidth}, InMax, InBorderColor); // Bottom
            DrawQuad(InMin, {InMin.x + InBorderWidth, InMax.y}, InBorderColor); // Left
            DrawQuad({InMax.x - InBorderWidth, InMin.y}, InMax, InBorderColor); // Right
        }
    }

    glm::vec2 FUIRenderer::MeasureString(const std::string& InText, float InScale) {
        float width = 0.0f;
        float maxWidth = 0.0f;
        float height = kUIFontPixelHeight * InScale;

        for (char c : InText) {
            if (c == '\n') {
                if (width > maxWidth)
                    maxWidth = width;
                width = 0.0f;
                height += kUILineHeight * InScale;
                continue;
            }
            if (c < 32 || c >= 128)
                continue;

            stbtt_bakedchar b = UIBakedChars[c - 32];
            width += b.xadvance * InScale;
        }
        if (width > maxWidth)
            maxWidth = width;
        return {maxWidth, height};
    }

    void FUIRenderer::DrawString(float InX, float InY, const std::string& InText, const glm::vec4& InColor,
                                 float InScale, ETextAlignment InAlignment) {
        if (InText.empty() || !bUIFontLoaded)
            return;

        const float scale = InScale > 0.0f ? InScale : 1.0f;
        const glm::vec2 measured = MeasureString(InText, scale);
        float originX = InX;
        if (InAlignment == ETextAlignment::Center)
            originX = InX - measured.x * 0.5f;
        else if (InAlignment == ETextAlignment::Right)
            originX = InX - measured.x;

        float penX = 0.0f;
        float penY = 0.0f;
        float baseline = InY + kUIBaseline * scale;

        for (char c : InText) {
            if (c == '\n') {
                penX = 0.0f;
                penY = 0.0f;
                baseline += kUILineHeight * scale;
                continue;
            }
            if (c < 32 || c >= 128)
                continue;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(UIBakedChars, kUIAtlasDim, kUIAtlasDim, c - 32, &penX, &penY, &q, 1);

            const float gx0 = originX + q.x0 * scale;
            const float gy0 = baseline + q.y0 * scale;
            const float gx1 = originX + q.x1 * scale;
            const float gy1 = baseline + q.y1 * scale;

            if (TextVertices.size() + 6 >= MaxUIVertices)
                break;

            TextVertices.push_back({{gx0, gy0}, {q.s0, q.t0}, InColor});
            TextVertices.push_back({{gx1, gy0}, {q.s1, q.t0}, InColor});
            TextVertices.push_back({{gx1, gy1}, {q.s1, q.t1}, InColor});
            TextVertices.push_back({{gx0, gy0}, {q.s0, q.t0}, InColor});
            TextVertices.push_back({{gx1, gy1}, {q.s1, q.t1}, InColor});
            TextVertices.push_back({{gx0, gy1}, {q.s0, q.t1}, InColor});
        }
    }

    void FUIRenderer::Flush(bool bUseTexture) {
        if (!Shader)
            return;

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(OrthoMatrix));
        Shader->SetInt("u_UseTexture", bUseTexture ? 1 : 0);

        if (bUseTexture) {
            Shader->SetInt("u_FontTexture", 0);
            if (FontTexture)
                FontTexture->Bind(0);
        }

        const auto& vertices = bUseTexture ? TextVertices : QuadVertices;
        if (vertices.empty())
            return;

        VertexBuffer->SetData(vertices.data(), static_cast<unsigned int>(vertices.size() * sizeof(FUIVertex)));
        VertexArray->Bind();
        FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(vertices.size()));
    }

    void FUIRenderer::FlushTextured(const TRef<FTexture2D>& InTexture, const std::vector<FUIVertex>& InVertices) {
        if (!Shader || !InTexture || InVertices.empty())
            return;

        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(OrthoMatrix));
        Shader->SetInt("u_UseTexture", 1);
        Shader->SetInt("u_FontTexture", 0);
        InTexture->Bind(0);

        VertexBuffer->SetData(InVertices.data(), static_cast<unsigned int>(InVertices.size() * sizeof(FUIVertex)));
        VertexArray->Bind();
        FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(InVertices.size()));
    }

    void FUIRenderer::End() {
        if (!Shader)
            return;

        // Configure UI pipeline states (Depth test OFF, Blending ON)
        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);

        // 1. Draw all opaque and tinted background quads
        if (!QuadVertices.empty()) {
            Flush(false);
            QuadVertices.clear();
        }

        // 2. Draw all text characters with font atlas texture
        if (!TextVertices.empty()) {
            Flush(true);
            TextVertices.clear();
        }

        // Restore default state so the next frame does not inherit UI blend
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
    }

} // namespace Leon
