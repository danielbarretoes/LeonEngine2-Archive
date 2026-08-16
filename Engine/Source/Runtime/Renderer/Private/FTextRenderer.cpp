#include "Renderer/FTextRenderer.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderCommand.hpp"

#include <cmath>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stb_truetype.h>

namespace Leon {

    TRef<FShader> FTextRenderer::Shader = nullptr;
    TRef<FTexture2D> FTextRenderer::FontTexture = nullptr;
    TRef<FVertexArray> FTextRenderer::VertexArray = nullptr;
    TRef<FVertexBuffer> FTextRenderer::VertexBuffer = nullptr;

    glm::mat4 FTextRenderer::ViewProjection = glm::mat4(1.0f);
    std::vector<FTextRenderer::FTextVertex> FTextRenderer::Vertices;

    static stbtt_bakedchar WorldBakedChars[96];
    static bool bFontLoaded = false;
    static constexpr float FontPixelHeight = 48.0f;
    static constexpr int AtlasDimension = 1024;

    void FTextRenderer::Init() {
        LE_CORE_INFO("Initializing 3D In-World TextRenderer Subsystem...");

        Shader = FShader::Create("Engine/Assets/Shaders/WorldText.glsl");

        std::vector<unsigned char> tempBitmap(AtlasDimension * AtlasDimension, 0);

        // Load Inter-Bold TrueType Font from Engine Assets (with fallback to Inter-Regular)
        std::ifstream file("Engine/Assets/Fonts/Inter-Bold.ttf", std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            file.open("Engine/Assets/Fonts/Inter-Regular.ttf", std::ios::binary | std::ios::ate);
        }
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<unsigned char> fontBuffer(size);
            if (file.read(reinterpret_cast<char*>(fontBuffer.data()), size)) {
                int res = stbtt_BakeFontBitmap(fontBuffer.data(), 0, FontPixelHeight, tempBitmap.data(), AtlasDimension,
                                               AtlasDimension, 32, 96, WorldBakedChars);
                if (res > 0) {
                    bFontLoaded = true;
                    LE_CORE_INFO("Baked High-Res 3D Bold Font Atlas (1024x1024, Inter-Bold 48px)");
                }
            }
        }

        if (!bFontLoaded) {
            LE_CORE_WARN("Could not bake Inter font for 3D Text; generating fallback.");
        }

        // Convert 1-channel alpha font bitmap to 4-channel RGBA texture
        std::vector<unsigned char> rgbaAtlas(AtlasDimension * AtlasDimension * 4, 0);
        for (size_t i = 0; i < tempBitmap.size(); ++i) {
            unsigned char a = tempBitmap[i];
            rgbaAtlas[i * 4 + 0] = 255;
            rgbaAtlas[i * 4 + 1] = 255;
            rgbaAtlas[i * 4 + 2] = 255;
            rgbaAtlas[i * 4 + 3] = a;
        }

        FontTexture = FTexture2D::Create(AtlasDimension, AtlasDimension);
        FontTexture->SetData(rgbaAtlas.data(), static_cast<unsigned int>(rgbaAtlas.size()));

        VertexArray = FVertexArray::Create();
        VertexBuffer = FVertexBuffer::Create(MaxVertices * sizeof(FTextVertex));
        VertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                   {EShaderDataType::Float2, "aTexCoord"},
                                   {EShaderDataType::Float4, "aColor"}});
        VertexArray->AddVertexBuffer(VertexBuffer);

        Vertices.reserve(4096);
    }

    void FTextRenderer::Shutdown() {
        Shader.reset();
        FontTexture.reset();
        VertexBuffer.reset();
        VertexArray.reset();
        Vertices.clear();
    }

    void FTextRenderer::BeginScene(const FPerspectiveCamera& InCamera) {
        ViewProjection = InCamera.GetViewProjectionMatrix();
        Vertices.clear();
    }

    void FTextRenderer::EndScene() {
        Flush();
    }

    void FTextRenderer::Flush() {
        if (Vertices.empty() || !Shader || !FontTexture)
            return;

        // Enable Alpha Blending with depth testing
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetDepthMask(true);

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(ViewProjection));
        Shader->SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f))); // Vertices are in world space

        FontTexture->Bind(0);
        Shader->SetInt("u_FontAtlas", 0);

        VertexBuffer->SetData(Vertices.data(), static_cast<unsigned int>(Vertices.size() * sizeof(FTextVertex)));

        VertexArray->Bind();
        FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(Vertices.size()));

        Vertices.clear();
    }

    glm::vec2 FTextRenderer::MeasureString(const std::string& InText, float InSize) {
        std::string processedText = InText;
        size_t pos = 0;
        while ((pos = processedText.find("\\n", pos)) != std::string::npos) {
            processedText.replace(pos, 2, "\n");
            pos += 1;
        }

        float scale = InSize / FontPixelHeight;
        float maxWidth = 0.0f;
        float currentWidth = 0.0f;
        int lineCount = 1;

        for (char c : processedText) {
            if (c == '\n') {
                maxWidth = std::max(maxWidth, currentWidth);
                currentWidth = 0.0f;
                lineCount++;
                continue;
            }

            if (c >= 32 && c < 128) {
                const auto& b = WorldBakedChars[c - 32];
                currentWidth += b.xadvance * scale;
            }
        }
        maxWidth = std::max(maxWidth, currentWidth);
        float totalHeight = lineCount * InSize * 1.2f;

        return glm::vec2(maxWidth, totalHeight);
    }

    void FTextRenderer::DrawString(const std::string& InText, const glm::mat4& InTransform, const glm::vec4& InColor,
                                   float InSize, ETextAlignment InAlignment, bool InbDoubleSided) {
        if (InText.empty())
            return;

        std::string processedText = InText;
        size_t pos = 0;
        while ((pos = processedText.find("\\n", pos)) != std::string::npos) {
            processedText.replace(pos, 2, "\n");
            pos += 1;
        }

        float scale = InSize / FontPixelHeight;
        float lineHeight = InSize * 1.2f;

        // Split text by lines
        std::vector<std::string> lines;
        std::istringstream stream(processedText);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }

        if (lines.empty())
            return;

        float totalBlockHeight = static_cast<float>(lines.size()) * lineHeight;
        float startY = (totalBlockHeight * 0.5f) - (lineHeight * 0.5f);

        for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
            const std::string& currentLine = lines[lineIdx];
            float currentLineY = startY - static_cast<float>(lineIdx) * lineHeight;

            // Measure line width for horizontal alignment
            float lineWidth = 0.0f;
            for (char c : currentLine) {
                if (c >= 32 && c < 128) {
                    lineWidth += WorldBakedChars[c - 32].xadvance * scale;
                }
            }

            float cursorX = 0.0f;
            if (InAlignment == ETextAlignment::Center) {
                cursorX = -lineWidth * 0.5f;
            } else if (InAlignment == ETextAlignment::Right) {
                cursorX = -lineWidth;
            }

            for (char c : currentLine) {
                if (c < 32 || c >= 128)
                    continue;

                const auto& b = WorldBakedChars[c - 32];

                float x0 = cursorX + b.xoff * scale;
                float y0 = currentLineY - b.yoff * scale;
                float x1 = x0 + (b.x1 - b.x0) * scale;
                float y1 = y0 - (b.y1 - b.y0) * scale;

                float u0 = static_cast<float>(b.x0) / static_cast<float>(AtlasDimension);
                float v0 = static_cast<float>(b.y0) / static_cast<float>(AtlasDimension);
                float u1 = static_cast<float>(b.x1) / static_cast<float>(AtlasDimension);
                float v1 = static_cast<float>(b.y1) / static_cast<float>(AtlasDimension);

                // Transform local 3D glyph corners to world space
                glm::vec3 p0 = glm::vec3(InTransform * glm::vec4(x0, y0, 0.0f, 1.0f));
                glm::vec3 p1 = glm::vec3(InTransform * glm::vec4(x1, y0, 0.0f, 1.0f));
                glm::vec3 p2 = glm::vec3(InTransform * glm::vec4(x1, y1, 0.0f, 1.0f));
                glm::vec3 p3 = glm::vec3(InTransform * glm::vec4(x0, y1, 0.0f, 1.0f));

                if (Vertices.size() + (InbDoubleSided ? 12 : 6) >= MaxVertices) {
                    Flush();
                }

                // Front Face (2 triangles = 6 vertices)
                Vertices.push_back({p0, {u0, v0}, InColor});
                Vertices.push_back({p1, {u1, v0}, InColor});
                Vertices.push_back({p2, {u1, v1}, InColor});

                Vertices.push_back({p0, {u0, v0}, InColor});
                Vertices.push_back({p2, {u1, v1}, InColor});
                Vertices.push_back({p3, {u0, v1}, InColor});

                // Back Face (Double Sided)
                if (InbDoubleSided) {
                    Vertices.push_back({p0, {u0, v0}, InColor});
                    Vertices.push_back({p2, {u1, v1}, InColor});
                    Vertices.push_back({p1, {u1, v0}, InColor});

                    Vertices.push_back({p0, {u0, v0}, InColor});
                    Vertices.push_back({p3, {u0, v1}, InColor});
                    Vertices.push_back({p2, {u1, v1}, InColor});
                }

                cursorX += b.xadvance * scale;
            }
        }
    }

} // namespace Leon
