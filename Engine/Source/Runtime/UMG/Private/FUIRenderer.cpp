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

    void FUIRenderer::Init() {
        if (bInitialized)
            return;

        LE_CORE_INFO("Initializing UI Renderer Subsystem...");
        Shader = FShader::Create("Engine/Assets/Shaders/DebugFont.glsl");

        constexpr int AtlasDim = 512;
        std::vector<unsigned char> tempBitmap(AtlasDim * AtlasDim, 0);

        std::ifstream file("Engine/Assets/Fonts/Inter-Regular.ttf", std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<unsigned char> fontBuffer(size);
            if (file.read(reinterpret_cast<char*>(fontBuffer.data()), size)) {
                int res = stbtt_BakeFontBitmap(fontBuffer.data(), 0, 18.0f, tempBitmap.data(), AtlasDim, AtlasDim, 32,
                                               96, UIBakedChars);
                if (res > 0) {
                    bUIFontLoaded = true;
                }
            }
        }

        std::vector<unsigned char> rgbaAtlas(AtlasDim * AtlasDim * 4, 0);
        for (size_t i = 0; i < tempBitmap.size(); ++i) {
            unsigned char a = tempBitmap[i];
            rgbaAtlas[i * 4 + 0] = a;
            rgbaAtlas[i * 4 + 1] = a;
            rgbaAtlas[i * 4 + 2] = a;
            rgbaAtlas[i * 4 + 3] = a;
        }

        FontTexture = FTexture2D::Create(AtlasDim, AtlasDim);
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
        float height = 18.0f * InScale;

        for (char c : InText) {
            if (c == '\n') {
                if (width > maxWidth)
                    maxWidth = width;
                width = 0.0f;
                height += 20.0f * InScale;
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
        if (InText.empty())
            return;

        glm::vec2 measured = MeasureString(InText, InScale);
        float startX = InX;
        if (InAlignment == ETextAlignment::Center) {
            startX = InX - (measured.x * 0.5f);
        } else if (InAlignment == ETextAlignment::Right) {
            startX = InX - measured.x;
        }

        float curX = startX;
        float curY = InY + 14.0f * InScale; // Baseline offset

        for (char c : InText) {
            if (c == '\n') {
                curX = startX;
                curY += 20.0f * InScale;
                continue;
            }
            if (c < 32 || c >= 128)
                continue;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(UIBakedChars, 512, 512, c - 32, &curX, &curY, &q, 1);

            // Scale around base position
            glm::vec2 p0 = {q.x0, q.y0};
            glm::vec2 p1 = {q.x1, q.y1};
            if (std::abs(InScale - 1.0f) > 0.001f) {
                p0 = {curX + (q.x0 - curX) * InScale, curY + (q.y0 - curY) * InScale};
                p1 = {curX + (q.x1 - curX) * InScale, curY + (q.y1 - curY) * InScale};
            }

            if (TextVertices.size() + 6 >= MaxUIVertices)
                break;

            TextVertices.push_back({p0, {q.s0, q.t0}, InColor});
            TextVertices.push_back({{p1.x, p0.y}, {q.s1, q.t0}, InColor});
            TextVertices.push_back({p1, {q.s1, q.t1}, InColor});

            TextVertices.push_back({p0, {q.s0, q.t0}, InColor});
            TextVertices.push_back({p1, {q.s1, q.t1}, InColor});
            TextVertices.push_back({{p0.x, p1.y}, {q.s0, q.t1}, InColor});
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

        // Restore default state
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);
    }

} // namespace Leon
