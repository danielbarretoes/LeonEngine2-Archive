#include "Renderer/FDebugOverlay.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderCommand.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Leon {

    TRef<FShader> FDebugOverlay::Shader = nullptr;
    TRef<FTexture2D> FDebugOverlay::FontTexture = nullptr;
    TRef<FVertexArray> FDebugOverlay::VertexArray = nullptr;
    TRef<FVertexBuffer> FDebugOverlay::VertexBuffer = nullptr;
    std::vector<FDebugOverlay::FOverlayVertex> FDebugOverlay::Vertices;

    float FDebugOverlay::SmoothedFPS = 60.0f;
    float FDebugOverlay::SmoothedFrameTimeMs = 16.6f;
    FGPUInfo FDebugOverlay::CachedGPUInfo;
    bool FDebugOverlay::bHasGPUInfo = false;

    static stbtt_bakedchar BakedChars[96];
    static bool bInterFontLoaded = false;

    static float BytesToMB(size_t InBytes) {
        return static_cast<float>(InBytes) / (1024.0f * 1024.0f);
    }

    static const FGPUMemoryBucket& GPUCategory(const FRenderStats& InStats, EGPUMemoryCategory InCategory) {
        return InStats.GPUMemory[static_cast<size_t>(InCategory)];
    }

    static void AppendMB(std::string& InOut, const char* InLabel, size_t InBytes) {
        if (InBytes == 0)
            return;
        if (!InOut.empty())
            InOut += "  ";
        InOut += std::format("{} {:.1f}", InLabel, BytesToMB(InBytes));
    }

    void FDebugOverlay::Init() {
        LE_CORE_INFO("Initializing DebugOverlay HUD Subsystem...");

        Shader = FShader::Create("Engine/Assets/Shaders/DebugFont.glsl");

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
                                               96, BakedChars);
                if (res > 0) {
                    bInterFontLoaded = true;
                    LE_CORE_INFO("Loaded Inter Font from Engine/Assets/Fonts/Inter-Regular.ttf");
                }
            }
        }

        if (!bInterFontLoaded) {
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

        FontTexture = FTexture2D::Create(AtlasDim, AtlasDim);
        FontTexture->SetData(rgbaAtlas.data(), static_cast<unsigned int>(rgbaAtlas.size()));

        VertexArray = FVertexArray::Create();
        VertexBuffer = FVertexBuffer::Create(MaxOverlayVertices * sizeof(FOverlayVertex));
        VertexBuffer->SetLayout({{EShaderDataType::Float2, "aPos"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float4, "aColor"}});
        VertexArray->AddVertexBuffer(VertexBuffer);

        Vertices.reserve(2048);
    }

    void FDebugOverlay::Shutdown() {
        Shader.reset();
        FontTexture.reset();
        VertexBuffer.reset();
        VertexArray.reset();
        Vertices.clear();
    }

    void FDebugOverlay::DrawQuad(const glm::vec2& InMin, const glm::vec2& InMax, const glm::vec4& InColor,
                                 const glm::vec2& InUVMin, const glm::vec2& InUVMax) {
        if (Vertices.size() + 6 >= MaxOverlayVertices)
            return;

        // 2 triangles (6 vertices)
        Vertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        Vertices.push_back({{InMax.x, InMin.y}, {InUVMax.x, InUVMin.y}, InColor});
        Vertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});

        Vertices.push_back({{InMin.x, InMin.y}, {InUVMin.x, InUVMin.y}, InColor});
        Vertices.push_back({{InMax.x, InMax.y}, {InUVMax.x, InUVMax.y}, InColor});
        Vertices.push_back({{InMin.x, InMax.y}, {InUVMin.x, InUVMax.y}, InColor});
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
            stbtt_GetBakedQuad(BakedChars, 512, 512, c - 32, &curX, &curY, &q, 1);

            DrawQuad({q.x0, q.y0}, {q.x1, q.y1}, InColor, {q.s0, q.t0}, {q.s1, q.t1});
        }
    }

    void FDebugOverlay::Flush(const glm::mat4& InOrthoMatrix) {
        if (Vertices.empty() || !Shader)
            return;

        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(InOrthoMatrix));
        Shader->SetInt("u_UseTexture", 1);
        Shader->SetInt("u_FontTexture", 0);

        if (FontTexture)
            FontTexture->Bind(0);

        VertexBuffer->SetData(Vertices.data(), static_cast<unsigned int>(Vertices.size() * sizeof(FOverlayVertex)));

        VertexArray->Bind();
        FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(Vertices.size()));

        Vertices.clear();
    }

    void FDebugOverlay::Render(unsigned int InViewportWidth, unsigned int InViewportHeight, FTimestep InDeltaTime,
                               const FRenderStats& InRenderStats, bool InbShowGizmos) {
        if (!Shader)
            return;

        // Smooth frame metrics over time
        float dt = InDeltaTime.GetSeconds();
        if (dt > 0.0001f) {
            float instantFPS = 1.0f / dt;
            float instantMs = dt * 1000.0f;
            SmoothedFPS = SmoothedFPS * 0.92f + instantFPS * 0.08f;
            SmoothedFrameTimeMs = SmoothedFrameTimeMs * 0.92f + instantMs * 0.08f;
        }

        // Cache GPU info once
        if (!bHasGPUInfo) {
            CachedGPUInfo = FPlatformMemory::GetGPUInfo();
            // Clean up long device driver suffixes
            std::string& r = CachedGPUInfo.Renderer;
            if (r.find("/") != std::string::npos) {
                r = r.substr(0, r.find("/"));
            }
            bHasGPUInfo = true;
        }

        FMemoryStats memStats = FPlatformMemory::GetMemoryStats();
        float ramMB = static_cast<float>(memStats.WorkingSetBytes) / (1024.0f * 1024.0f);
        float peakRamMB = static_cast<float>(memStats.PeakWorkingSetBytes) / (1024.0f * 1024.0f);
        float vramTotalMB = static_cast<float>(memStats.DedicatedVideoMemoryBytes) / (1024.0f * 1024.0f);
        float vramUsedMB = static_cast<float>(memStats.UsedVideoMemoryBytes) / (1024.0f * 1024.0f);

        glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(InViewportWidth), static_cast<float>(InViewportHeight),
                                     0.0f, -1.0f, 1.0f);

        // Disable depth testing and enable standard alpha blending for HUD overlay
        // Culling must be off: after FUIRenderer::End(), Back-face culling is restored and
        // Y-flipped ortho quads would be culled (F1 overlay invisible).
        FRenderCommand::SetWireframe(false);
        FRenderCommand::SetDepthTesting(false);
        FRenderCommand::SetCulling(false);
        FRenderCommand::SetBlendState(true);
        FRenderCommand::SetBlendFunc(EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha);

        Vertices.clear();

        // 1. Draw Glassmorphism Dark HUD Panel Background
        glm::vec2 boxMin(16.0f, 16.0f);
        glm::vec2 boxMax(600.0f, 330.0f);

        // Backdrop quad (untextured solid)
        Shader->Bind();
        Shader->SetMat4("u_ViewProjection", glm::value_ptr(ortho));
        Shader->SetInt("u_UseTexture", 0);

        glm::vec4 bgColor(0.05f, 0.06f, 0.09f, 0.82f);
        DrawQuad(boxMin, boxMax, bgColor);

        // Header Accent Bar (Cyan, 2.5px)
        DrawQuad(boxMin, {boxMax.x, boxMin.y + 2.5f}, glm::vec4(0.0f, 0.85f, 1.0f, 0.95f));

        // Flush panel geometry
        if (!Vertices.empty()) {
            VertexBuffer->SetData(Vertices.data(), static_cast<unsigned int>(Vertices.size() * sizeof(FOverlayVertex)));
            VertexArray->Bind();
            FRenderCommand::DrawArrays(VertexArray, static_cast<unsigned int>(Vertices.size()));
            Vertices.clear();
        }

        // 2. Draw Text Lines with Inter Font Texture
        float textX = 26.0f;
        float textY = 24.0f;
        float lineHeight = 18.0f;

        // Title
        DrawString(textX, textY, "LEON ENGINE 2 - HUD DIAGNOSTICS", glm::vec4(0.0f, 0.9f, 1.0f, 1.0f));
        textY += 21.0f;

        // FPS and Frame Time
        glm::vec4 fpsColor = (SmoothedFPS >= 55.0f)   ? glm::vec4(0.2f, 1.0f, 0.4f, 1.0f)
                             : (SmoothedFPS >= 30.0f) ? glm::vec4(1.0f, 0.8f, 0.2f, 1.0f)
                                                      : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        DrawString(textX, textY, std::format("FPS: {:.1f} ({:.2f} ms)", SmoothedFPS, SmoothedFrameTimeMs), fpsColor);
        textY += lineHeight;

        // RAM (Engine Process)
        DrawString(textX, textY, std::format("RAM: {:.1f} MB (Peak: {:.1f} MB)", ramMB, peakRamMB),
                   glm::vec4(0.92f, 0.92f, 0.92f, 1.0f));
        textY += lineHeight;

        // VRAM (engine-tracked GPU resources vs DXGI/GL used)
        float engineVramMB = BytesToMB(InRenderStats.AllocatedGPUMemoryBytes);
        if (vramTotalMB > 0.0f) {
            DrawString(textX, textY,
                       std::format("VRAM: {:.1f} MB  GPU {:.0f}/{:.0f} MB", engineVramMB, vramUsedMB, vramTotalMB),
                       glm::vec4(0.92f, 0.92f, 0.92f, 1.0f));
        } else {
            DrawString(textX, textY, std::format("VRAM: {:.1f} MB", engineVramMB),
                       glm::vec4(0.92f, 0.92f, 0.92f, 1.0f));
        }
        textY += lineHeight;

        const size_t meshBytes = GPUCategory(InRenderStats, EGPUMemoryCategory::VertexBuffer).Bytes +
                                 GPUCategory(InRenderStats, EGPUMemoryCategory::IndexBuffer).Bytes;
        std::string categoryLine;
        AppendMB(categoryLine, "Tex", GPUCategory(InRenderStats, EGPUMemoryCategory::Texture2D).Bytes);
        AppendMB(categoryLine, "Cube", GPUCategory(InRenderStats, EGPUMemoryCategory::TextureCube).Bytes);
        AppendMB(categoryLine, "FBO", GPUCategory(InRenderStats, EGPUMemoryCategory::Framebuffer).Bytes);
        AppendMB(categoryLine, "Mesh", meshBytes);
        AppendMB(categoryLine, "UBO", GPUCategory(InRenderStats, EGPUMemoryCategory::UniformBuffer).Bytes);
        AppendMB(categoryLine, "Swap", GPUCategory(InRenderStats, EGPUMemoryCategory::Swapchain).Bytes);
        if (!categoryLine.empty()) {
            DrawString(textX, textY, categoryLine, glm::vec4(0.72f, 0.78f, 0.86f, 1.0f));
            textY += lineHeight;
        }

        if (!InRenderStats.NamedGPUMemory.empty()) {
            std::vector<std::pair<std::string, size_t>> named;
            named.reserve(InRenderStats.NamedGPUMemory.size());
            for (const auto& [label, bucket] : InRenderStats.NamedGPUMemory) {
                if (bucket.Bytes > 0)
                    named.emplace_back(label, bucket.Bytes);
            }
            std::sort(named.begin(), named.end(),
                      [](const auto& InA, const auto& InB) { return InA.second > InB.second; });

            std::string namedLine;
            constexpr size_t kMaxNamed = 6;
            for (size_t i = 0; i < named.size() && i < kMaxNamed; ++i)
                AppendMB(namedLine, named[i].first.c_str(), named[i].second);
            if (!namedLine.empty()) {
                DrawString(textX, textY, namedLine, glm::vec4(0.62f, 0.70f, 0.80f, 1.0f));
                textY += lineHeight;
            }
        }

        // GPU & Driver
        DrawString(textX, textY, std::format("GPU: {}", CachedGPUInfo.Renderer), glm::vec4(0.85f, 0.9f, 1.0f, 1.0f));
        textY += lineHeight;

        // Viewport Resolution
        DrawString(textX, textY, std::format("Resolution: {} x {}", InViewportWidth, InViewportHeight),
                   glm::vec4(0.85f, 0.85f, 0.85f, 1.0f));
        textY += lineHeight;

        // Render Statistics (Draw Calls, Tris, Vertices)
        DrawString(textX, textY,
                   std::format("Draw Calls: {} | Tris: {} | Culled: {}/{}", InRenderStats.DrawCalls,
                               InRenderStats.TriangleCount, InRenderStats.MeshesCulled,
                               InRenderStats.MeshesCulled + InRenderStats.MeshesDrawn),
                   glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
        textY += lineHeight;
        if (InRenderStats.StaticMeshSourceTriangles > 0 || InRenderStats.StaticMeshSubmittedTriangles > 0) {
            DrawString(textX, textY,
                       std::format("LOD {}/{}/{}/{}/{}  src {}  lod {}", InRenderStats.StaticMeshLODCounts[0],
                                   InRenderStats.StaticMeshLODCounts[1], InRenderStats.StaticMeshLODCounts[2],
                                   InRenderStats.StaticMeshLODCounts[3], InRenderStats.StaticMeshLODCounts[4],
                                   InRenderStats.StaticMeshSourceTriangles, InRenderStats.StaticMeshSubmittedTriangles),
                       glm::vec4(0.85f, 0.78f, 0.45f, 1.0f));
            textY += lineHeight;
        }

        const auto& timing = FFrameProfiler::Last();
        DrawString(textX, textY,
                   std::format("Game {:.1f}  Render {:.1f}  GPU {:.1f}  Phys {:.1f}  AI {:.1f}  Anim {:.1f}",
                               timing.GameMs, timing.RenderMs, timing.GPUMs, timing.PhysicsMs, timing.AIMs,
                               timing.AnimationMs),
                   glm::vec4(0.85f, 0.95f, 1.0f, 1.0f));
        textY += lineHeight;
        DrawString(
            textX, textY,
            std::format("Shadow {:.1f}  Opaque {:.1f}  Planar {:.1f}  Sky {:.1f}  PP {:.1f}  UI {:.1f}  Present {:.1f}",
                        timing.ShadowMs, timing.OpaqueMs, timing.PlanarMs, timing.SkyMs, timing.PostProcessMs,
                        timing.UIMs, timing.PresentMs),
            glm::vec4(0.75f, 0.85f, 0.95f, 1.0f));
        textY += lineHeight;
        DrawString(textX, textY,
                   std::format("GPU sh {:.1f} op {:.1f} pl {:.1f} pp {:.1f}  Tick A/C {}/{}  Paths {}  ShDraw {}",
                               timing.GPUShadowMs, timing.GPUOpaqueMs, timing.GPUPlanarMs, timing.GPUPostProcessMs,
                               timing.TickActors, timing.TickComponents, timing.PathRequests, timing.ShadowDrawCalls),
                   glm::vec4(0.75f, 0.85f, 0.95f, 1.0f));
        textY += lineHeight;

        // Hotkey state guide
        std::string hotkeyInfo = std::format("[F1] HUD: ON   [F2] Gizmos: {}", InbShowGizmos ? "ON" : "OFF");
        glm::vec4 gizmoColor = InbShowGizmos ? glm::vec4(0.2f, 1.0f, 0.5f, 1.0f) : glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        DrawString(textX, textY, hotkeyInfo, gizmoColor);

        Flush(ortho);

        // Restore defaults so the next frame does not inherit overlay blend
        FRenderCommand::SetDepthTesting(true);
        FRenderCommand::SetCulling(true, ECullMode::Back);
        FRenderCommand::SetBlendState(false);
    }

} // namespace Leon
