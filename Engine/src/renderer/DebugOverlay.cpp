#include "renderer/DebugOverlay.hpp"
#include "core/Log.hpp"
#include "renderer/RenderCommand.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Leon {

    TRef<FShader> FDebugOverlay::s_Shader = nullptr;
    TRef<FTexture2D> FDebugOverlay::s_FontTexture = nullptr;
    TRef<FVertexArray> FDebugOverlay::s_VertexArray = nullptr;
    TRef<FVertexBuffer> FDebugOverlay::s_VertexBuffer = nullptr;
    std::vector<FDebugOverlay::FOverlayVertex> FDebugOverlay::s_Vertices;

    float FDebugOverlay::s_SmoothedFPS = 60.0f;
    float FDebugOverlay::s_SmoothedFrameTimeMs = 16.6f;
    FGPUInfo FDebugOverlay::s_CachedGPUInfo;
    bool FDebugOverlay::s_bHasGPUInfo = false;

    static stbtt_bakedchar s_BakedChars[96];
    static bool s_bInterFontLoaded = false;

    void FDebugOverlay::Init() {
        LE_CORE_INFO("Initializing DebugOverlay HUD Subsystem...");

        s_Shader = FShader::Create("Engine/Assets/Shaders/DebugFont.glsl");

        constexpr int AtlasDim = 512;
        std::vector<unsigned char> tempBitmap(AtlasDim * AtlasDim, 0);

        // Load Inter TrueType Font from Engine Assets
        std::ifstream file("Engine/Assets/Fonts/Inter-Regular.ttf", std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<unsigned char> fontBuffer(size);
            if (file.read(reinterpret_cast<char*>(fontBuffer.data()), size)) {
                int res = stbtt_BakeFontBitmap(fontBuffer.data(), 0, 16.0f, tempBitmap.data(), AtlasDim, AtlasDim, 32,
                                               96, s_BakedChars);
                if (res > 0) {
                    s_bInterFontLoaded = true;
                    LE_CORE_INFO("Loaded Inter Font from Engine/Assets/Fonts/Inter-Regular.ttf");
                }
            }
        }

        if (!s_bInterFontLoaded) {
            LE_CORE_WARN("Could not load Inter font; generating fallback font.");
        }

        // Convert 1-channel alpha font bitmap to 4-channel RGBA texture
        std::vector<unsigned char> rgbaAtlas(AtlasDim * AtlasDim * 4, 0);
        for (size_t i = 0; i < tempBitmap.size(); ++i) {
            unsigned char a = tempBitmap[i];
            rgbaAtlas[i * 4 + 0] = a;
            rgbaAtlas[i * 4 + 1] = a;
            rgbaAtlas[i * 4 + 2] = a;
            rgbaAtlas[i * 4 + 3] = a;
        }

        s_FontTexture = FTexture2D::Create(AtlasDim, AtlasDim);
        s_FontTexture->SetData(rgbaAtlas.data(), static_cast<unsigned int>(rgbaAtlas.size()));

        s_VertexArray = FVertexArray::Create();
        s_VertexBuffer = FVertexBuffer::Create(MaxOverlayVertices * sizeof(FOverlayVertex));
        s_VertexBuffer->SetLayout({{EShaderDataType::Float2, "aPos"},
                                   {EShaderDataType::Float2, "aTexCoord"},
                                   {EShaderDataType::Float4, "aColor"}});
        s_VertexArray->AddVertexBuffer(s_VertexBuffer);

        s_Vertices.reserve(2048);
    }

    void FDebugOverlay::Shutdown() {
        s_Shader.reset();
        s_FontTexture.reset();
        s_VertexBuffer.reset();
        s_VertexArray.reset();
        s_Vertices.clear();
    }

    void FDebugOverlay::DrawQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InColor,
                                 const glm::vec2& InUVMin, const glm::vec2& InUVMax) {
        if (s_Vertices.size() + 6 >= MaxOverlayVertices)
            return;

        // 2 triangles (6 vertices)
        s_Vertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        s_Vertices.push_back({{InMax.x, InMin.y}, {InUVMax.x, InUVMin.y}, InColor});
        s_Vertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});

        s_Vertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        s_Vertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});
        s_Vertices.push_back({{InMin.x, InMax.y}, {InUVMin.x, InUVMax.y}, InColor});
    }

    void FDebugOverlay::DrawString(float InX, float InY, const std::string& InText, const glm::vec4& InColor,
                                   float InScale) {
        (void)InScale;
        float curX = InX;
        float curY = InY + 13.0f; // Baseline offset for TrueType baked font

        for (char c : InText) {
            if (c == '\n') {
                curX = InX;
                curY += 18.0f;
                continue;
            }

            if (c < 32 || c >= 128)
                continue;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(s_BakedChars, 512, 512, c - 32, &curX, &curY, &q, 1);

            DrawQuad({q.x0, q.y0}, {q.x1, q.y1}, InColor, {q.s0, q.t0}, {q.s1, q.t1});
        }
    }

    void FDebugOverlay::Flush(const glm::mat4& InOrthoMatrix) {
        if (s_Vertices.empty() || !s_Shader)
            return;

        s_Shader->Bind();
        s_Shader->SetMat4("u_ViewProjection", glm::value_ptr(InOrthoMatrix));
        s_Shader->SetInt("u_UseTexture", 1);
        s_Shader->SetInt("u_FontTexture", 0);

        if (s_FontTexture)
            s_FontTexture->Bind(0);

        s_VertexBuffer->SetData(s_Vertices.data(),
                                static_cast<unsigned int>(s_Vertices.size() * sizeof(FOverlayVertex)));

        s_VertexArray->Bind();
        FRenderCommand::DrawArrays(s_VertexArray, static_cast<unsigned int>(s_Vertices.size()));

        s_Vertices.clear();
    }

    void FDebugOverlay::Render(unsigned int InViewportWidth, unsigned int InViewportHeight, FTimestep InDeltaTime,
                               const FRenderStats& InRenderStats, bool InbShowGizmos) {
        if (!s_Shader)
            return;

        // Smooth frame metrics over time
        float dt = InDeltaTime.GetSeconds();
        if (dt > 0.0001f) {
            float instantFPS = 1.0f / dt;
            float instantMs = dt * 1000.0f;
            s_SmoothedFPS = s_SmoothedFPS * 0.92f + instantFPS * 0.08f;
            s_SmoothedFrameTimeMs = s_SmoothedFrameTimeMs * 0.92f + instantMs * 0.08f;
        }

        // Cache GPU info once
        if (!s_bHasGPUInfo) {
            s_CachedGPUInfo = FPlatformMemory::GetGPUInfo();
            // Clean up long device driver suffixes
            std::string& r = s_CachedGPUInfo.Renderer;
            if (r.find("/") != std::string::npos) {
                r = r.substr(0, r.find("/"));
            }
            s_bHasGPUInfo = true;
        }

        FMemoryStats memStats = FPlatformMemory::GetMemoryStats();
        float ramMB = static_cast<float>(memStats.WorkingSetBytes) / (1024.0f * 1024.0f);
        float peakRamMB = static_cast<float>(memStats.PeakWorkingSetBytes) / (1024.0f * 1024.0f);
        float vramTotalMB = static_cast<float>(memStats.DedicatedVideoMemoryBytes) / (1024.0f * 1024.0f);
        float vramUsedMB = static_cast<float>(memStats.UsedVideoMemoryBytes) / (1024.0f * 1024.0f);

        glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(InViewportWidth), static_cast<float>(InViewportHeight),
                                     0.0f, -1.0f, 1.0f);

        // Disable depth testing and enable standard alpha blending for HUD overlay
        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);

        s_Vertices.clear();

        // 1. Draw Glassmorphism Dark HUD Panel Background (Width: 440px, Height: 185px)
        glm::vec2 boxMin(16.0f, 16.0f);
        glm::vec2 boxMax(456.0f, 202.0f);

        // Backdrop quad (untextured solid)
        s_Shader->Bind();
        s_Shader->SetMat4("u_ViewProjection", glm::value_ptr(ortho));
        s_Shader->SetInt("u_UseTexture", 0);

        glm::vec4 bgColor(0.05f, 0.06f, 0.09f, 0.82f);
        DrawQuad(boxMin, boxMax, bgColor);

        // Header Accent Bar (Cyan, 2.5px)
        DrawQuad(boxMin, {boxMax.x, boxMin.y + 2.5f}, glm::vec4(0.0f, 0.85f, 1.0f, 0.95f));

        // Flush panel geometry
        if (!s_Vertices.empty()) {
            s_VertexBuffer->SetData(s_Vertices.data(),
                                    static_cast<unsigned int>(s_Vertices.size() * sizeof(FOverlayVertex)));
            s_VertexArray->Bind();
            FRenderCommand::DrawArrays(s_VertexArray, static_cast<unsigned int>(s_Vertices.size()));
            s_Vertices.clear();
        }

        // 2. Draw Text Lines with Inter Font Texture
        float textX = 26.0f;
        float textY = 24.0f;
        float lineHeight = 18.0f;

        // Title
        DrawString(textX, textY, "LEON ENGINE 2 - HUD DIAGNOSTICS", glm::vec4(0.0f, 0.9f, 1.0f, 1.0f));
        textY += 21.0f;

        // FPS and Frame Time
        glm::vec4 fpsColor = (s_SmoothedFPS >= 55.0f)   ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f)
                             : (s_SmoothedFPS >= 30.0f) ? glm::vec4(1.0f, 0.8f, 0.2f, 1.0f)
                                                        : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        DrawString(textX, textY, std::format("FPS: {:.1f} ({:.2f} ms)", s_SmoothedFPS, s_SmoothedFrameTimeMs),
                   fpsColor);
        textY += lineHeight;

        // RAM (Engine Process)
        DrawString(textX, textY, std::format("RAM: {:.1f} MB (Peak: {:.1f} MB)", ramMB, peakRamMB),
                   glm::vec4(0.92f, 0.92f, 0.92f, 1.0f));
        textY += lineHeight;

        // VRAM (Engine GPU Resources)
        float engineVramMB = static_cast<float>(InRenderStats.AllocatedGPUMemoryBytes) / (1024.0f * 1024.0f);
        DrawString(textX, textY, std::format("VRAM: {:.2f} MB", engineVramMB), glm::vec4(0.92f, 0.92f, 0.92f, 1.0f));
        textY += lineHeight;

        // GPU & Driver
        DrawString(textX, textY, std::format("GPU: {}", s_CachedGPUInfo.Renderer), glm::vec4(0.85f, 0.9f, 1.0f, 1.0f));
        textY += lineHeight;

        // Viewport Resolution
        DrawString(textX, textY, std::format("Resolution: {} x {}", InViewportWidth, InViewportHeight),
                   glm::vec4(0.85f, 0.85f, 0.85f, 1.0f));
        textY += lineHeight;

        // Render Statistics (Draw Calls, Tris, Vertices)
        DrawString(textX, textY,
                   std::format("Draw Calls: {} | Tris: {} | Verts: {}", InRenderStats.DrawCalls,
                               InRenderStats.TriangleCount, InRenderStats.VertexCount),
                   glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
        textY += lineHeight;

        // Hotkey state guide
        std::string hotkeyInfo = std::format("[F1] HUD: ON   [F2] Gizmos: {}", InbShowGizmos ? "ON" : "OFF");
        glm::vec4 gizmoColor = InbShowGizmos ? glm::vec4(0.2f, 1.0f, 0.5f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        DrawString(textX, textY, hotkeyInfo, gizmoColor);

        Flush(ortho);

        // Restore depth testing for subsequent frames / 3D passes
        FRenderCommand::SetDepthTesting(true);
    }

} // namespace Leon
