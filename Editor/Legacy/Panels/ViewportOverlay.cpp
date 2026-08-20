#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <leon/core/Camera.h>
#include <leon/core/MemoryStats.h>
#include <leon/debug/DebugView.h>
#include <leon/debug/LightDebugDraw.h>
#include <leon/editor/EditorTheme.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/Engine.h>
#include <leon/gameplay/GameMode.h>
#include <leon/level/Level.h>
#include <leon/level/TextRenderDraw.h>
#include <leon/physics/BodyInstance.h>
#include <leon/render/Renderer.h>
#include <leon/ui/TextLayout.h>
#include <leon/ui/WorldScreen.h>
#include <string>
#include <vector>

namespace leon::editor {

namespace {

ImU32 ToImColor(const glm::vec3& color, int alpha = 255) {
    const auto ch = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
    return IM_COL32(ch(color.r), ch(color.g), ch(color.b), alpha);
}

void DrawOutlinedText(ImDrawList* draw, const ImVec2& pos, ImU32 color, const char* text,
                      float fontSizePx) {
    if (draw == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }
    ImFont* font = ImGui::GetFont();
    if (font != nullptr && fontSizePx >= 1.0f) {
        draw->AddText(font, fontSizePx, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220),
                      text);
        draw->AddText(font, fontSizePx, pos, color, text);
        return;
    }
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
    draw->AddText(pos, color, text);
}

[[nodiscard]] float EditorLineStepPx(float fontSizePx) {
    return fontSizePx * (kHudLineHeight / kHudFontBakePx);
}

[[nodiscard]] float HorizontalOffset(ETextJustify justify, float lineWidth) {
    switch (justify) {
    case ETextJustify::Left:
        return 0.0f;
    case ETextJustify::Right:
        return -lineWidth;
    case ETextJustify::Center:
    default:
        return -lineWidth * 0.5f;
    }
}

[[nodiscard]] bool ProjectCorner(const glm::mat4& view, const glm::mat4& projection,
                                 const glm::vec3& world, float physicalW, float physicalH,
                                 glm::vec2& outPhysical, bool& inFront) {
    if (!ProjectWorldToScreenMatrices(view, projection, world, physicalW, physicalH, outPhysical,
                                      inFront) ||
        !inFront) {
        return false;
    }
    return outPhysical.x >= -256.0f && outPhysical.y >= -256.0f &&
           outPhysical.x <= physicalW + 256.0f && outPhysical.y <= physicalH + 256.0f;
}

void DrawWorldPlaneLine(ImDrawList* draw, ImFont* font, const std::string& line,
                        const glm::vec3& lineCenter, const glm::vec3& right, const glm::vec3& up,
                        float worldSize, float fontSizePx, ETextJustify justify,
                        const glm::mat4& view, const glm::mat4& projection, float physicalW,
                        float physicalH, float logicalScaleX, float logicalScaleY,
                        const ImVec2& imageMin, ImU32 color) {
    if (draw == nullptr || font == nullptr || line.empty() || fontSizePx < 1.0f) {
        return;
    }

    ImTextureID tex = font->ContainerAtlas->TexID;
    const float glyphScale = fontSizePx / font->FontSize;
    const float metersPerPx = (worldSize * 0.01f) / fontSizePx;
    const float ascentPx = font->Ascent * glyphScale;
    const float lineHeightPx = (font->Ascent - font->Descent) * glyphScale;
    const ImVec2 lineSize = font->CalcTextSizeA(fontSizePx, FLT_MAX, 0.0f, line.c_str());

    glm::vec3 baselineLeft =
        lineCenter - up * (lineHeightPx * metersPerPx * 0.5f) + up * (ascentPx * metersPerPx);
    if (justify == ETextJustify::Center) {
        baselineLeft -= right * (lineSize.x * metersPerPx * 0.5f);
    } else if (justify == ETextJustify::Right) {
        baselineLeft -= right * (lineSize.x * metersPerPx);
    }

    const auto toScreen = [&](const glm::vec3& world, ImVec2& out) {
        glm::vec2 physical{};
        bool inFront = false;
        if (!ProjectCorner(view, projection, world, physicalW, physicalH, physical, inFront)) {
            return false;
        }
        out = ImVec2{imageMin.x + physical.x * logicalScaleX,
                     imageMin.y + physical.y * logicalScaleY};
        return true;
    };

    float penX = 0.0f;
    for (char ch : line) {
        const ImFontGlyph* glyph =
            font->FindGlyph(static_cast<ImWchar>(static_cast<unsigned char>(ch)));
        if (glyph == nullptr) {
            continue;
        }

        const float x0 = penX + glyph->X0 * glyphScale;
        const float x1 = penX + glyph->X1 * glyphScale;
        const float y0 = glyph->Y0 * glyphScale;
        const float y1 = glyph->Y1 * glyphScale;

        const auto corner = [&](float bx, float by) {
            return baselineLeft + right * (bx * metersPerPx) + up * (-by * metersPerPx);
        };

        ImVec2 p0{};
        ImVec2 p1{};
        ImVec2 p2{};
        ImVec2 p3{};
        if (!toScreen(corner(x0, y0), p0) || !toScreen(corner(x1, y0), p1) ||
            !toScreen(corner(x1, y1), p2) || !toScreen(corner(x0, y1), p3)) {
            penX += glyph->AdvanceX * glyphScale;
            continue;
        }

        draw->AddImageQuad(tex, p0, p1, p2, p3, ImVec2{glyph->U0, glyph->V0},
                           ImVec2{glyph->U1, glyph->V0}, ImVec2{glyph->U1, glyph->V1},
                           ImVec2{glyph->U0, glyph->V1}, color);
        penX += glyph->AdvanceX * glyphScale;
    }
}

} // namespace

void ViewportPanel::DrawTextRenderOverlay(const Level& level, const ImVec2& imageMin,
                                          const ImVec2& imageSize) {
    if (!overlayMatricesValid_ || imageSize.x < 1.0f || imageSize.y < 1.0f || overlayFbW_ < 1 ||
        overlayFbH_ < 1) {
        return;
    }
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (draw == nullptr) {
        return;
    }

    const ImVec2 imageMax{imageMin.x + imageSize.x, imageMin.y + imageSize.y};
    draw->PushClipRect(imageMin, imageMax, true);

    const float physicalW = static_cast<float>(overlayFbW_);
    const float physicalH = static_cast<float>(overlayFbH_);
    const float logicalFromPhysicalX = imageSize.x / physicalW;
    const float logicalFromPhysicalY = imageSize.y / physicalH;

    for (const TextRenderActor& tr : level.TextRenderActors()) {
        if (tr.Text.empty()) {
            continue;
        }

        const float worldSize = std::max(1.0f, tr.WorldSize);
        const glm::vec3 right = TextRenderPlaneRight(tr.transform);
        const glm::vec3 up = TextRenderPlaneUp(tr.transform);
        const float lineStepWorld = TextRenderWorldLineStepMeters(worldSize);

        std::vector<std::string> lines;
        SplitTextRenderLines(tr.Text, lines);
        const float blockHeight =
            lineStepWorld * static_cast<float>(std::max<std::size_t>(1, lines.size()) - 1);

        for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const std::string& line = lines[lineIndex];
            if (line.empty()) {
                continue;
            }

            const float yOff = blockHeight * 0.5f - lineStepWorld * static_cast<float>(lineIndex);
            const glm::vec3 lineCenter = tr.transform.position + up * yOff;

            glm::vec2 screenPhysical{};
            bool inFront = false;
            if (!ProjectWorldToScreenMatrices(overlayView_, overlayProjection_, lineCenter,
                                              physicalW, physicalH, screenPhysical, inFront) ||
                !inFront) {
                continue;
            }

            const float fontSizePx = TextRenderScreenPixelHeightAtDistance(
                overlayEye_, overlayFov_, overlayOrtho_, lineCenter, worldSize, imageSize.y);

            const ImU32 textColor = ToImColor(tr.TextRenderColor);
            DrawWorldPlaneLine(draw, GetEditorTextRenderLabelFont(), line, lineCenter, right, up,
                               worldSize, fontSizePx, tr.HorizontalAlignment, overlayView_,
                               overlayProjection_, physicalW, physicalH, logicalFromPhysicalX,
                               logicalFromPhysicalY, imageMin, textColor);
        }
    }

    draw->PopClipRect();
}

void ViewportPanel::RebuildNavPreview(EditorContext& ctx) {
    if (ctx.level == nullptr) {
        navPreview_.Clear();
        navPreviewLevelToken_ = 0;
        return;
    }

    const std::uint64_t token = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(static_cast<const void*>(ctx.level)));
    if (token == navPreviewLevelToken_ && navPreview_.HasNavMesh()) {
        return;
    }

    collisionPreviewScene_.Clear();
    const auto& meshes = ctx.level->StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const StaticMeshComponent& component = meshes[i];
        if (!component.HasPhysicsBody()) {
            continue;
        }
        BodyInstanceDesc desc{};
        desc.levelMeshIndex = i;
        desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
        desc.enableGravity = component.enableGravity;
        collisionPreviewScene_.AddBody(desc);
    }
    collisionPreviewScene_.SyncFromLevel(*ctx.level);
    const float floorY = GameMode::EstimateFloorY(*ctx.level);
    const float walkBounds = GameMode::EstimateWalkBounds(*ctx.level);
    navPreview_.SetCellSize(0.5f);
    navPreview_.SetAgentRadius(0.45f);
    navPreview_.BuildFromLevel(*ctx.level, collisionPreviewScene_, floorY, walkBounds);
    navPreviewLevelToken_ = token;
}

void ViewportPanel::AppendEngineDebugOverlay(EditorContext& ctx) {
    if (ctx.engine == nullptr || ctx.renderer == nullptr || ctx.level == nullptr) {
        return;
    }

    Engine& engine = *ctx.engine;
    ctx.renderer->SetDebugDrawEnabled(engine.IsCollisionDebugEnabled());

    if (engine.IsCollisionDebugEnabled()) {
        collisionPreviewScene_.Clear();
        const auto& meshes = ctx.level->StaticMeshes();
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            const StaticMeshComponent& component = meshes[i];
            if (!component.HasPhysicsBody()) {
                continue;
            }
            BodyInstanceDesc desc{};
            desc.levelMeshIndex = i;
            desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
            desc.enableGravity = component.enableGravity;
            collisionPreviewScene_.AddBody(desc);
        }
        collisionPreviewScene_.SyncFromLevel(*ctx.level);
        collisionPreviewScene_.AppendBodiesCollisionDebug(ctx.renderer->GetDebugDraw());
    }

    if (engine.IsNavMeshDebugEnabled()) {
        RebuildNavPreview(ctx);
        navPreview_.AppendDebugDraw(ctx.renderer->GetDebugDraw());
    }

    if (engine.IsLightDebugEnabled()) {
        AppendLevelLightGizmos(*ctx.level, ctx.renderer->GetDebugDraw());
    }
}

void ViewportPanel::DrawDebugHintsOverlay(EditorContext& ctx, const ImVec2& imageMin) {
    if (ctx.engine == nullptr) {
        return;
    }
    const Engine& engine = *ctx.engine;
    std::array<char, 128> hints{};
    FormatDebugHotkeyHints(hints.data(), hints.size(), engine.IsNavMeshDebugEnabled(),
                           engine.IsLightDebugEnabled(), engine.IsHudStatsVisible(),
                           engine.IsCollisionDebugEnabled());

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (draw == nullptr) {
        return;
    }
    const ImVec2 pos{imageMin.x + 10.0f,
                     imageMin.y + static_cast<float>(std::max(ctx.viewportHeight, 1)) - 68.0f};
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180), hints.data());
    draw->AddText(pos, IM_COL32(200, 200, 200, 230), hints.data());
}

void ViewportPanel::AppendPlayerCollisionOverlay(EditorContext& ctx) {
    if (ctx.renderer == nullptr || ctx.level == nullptr) {
        return;
    }

    collisionPreviewScene_.Clear();
    const auto& meshes = ctx.level->StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const StaticMeshComponent& component = meshes[i];
        if (!component.HasPhysicsBody()) {
            continue;
        }
        BodyInstanceDesc desc{};
        desc.levelMeshIndex = i;
        desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
        desc.enableGravity = component.enableGravity;
        collisionPreviewScene_.AddBody(desc);
    }
    collisionPreviewScene_.SyncFromLevel(*ctx.level);
    collisionPreviewScene_.AppendBodiesCollisionDebug(ctx.renderer->GetDebugDraw());
}

void ViewportPanel::DrawViewModeOverlay(EditorContext& ctx, const ImVec2& imageMin) {
    if (ctx.viewportViewMode != EEditorViewportViewMode::PlayerCollision) {
        return;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (draw == nullptr) {
        return;
    }
    const char* label = "Player Collision";
    const ImVec2 pos{imageMin.x + 10.0f, imageMin.y + 10.0f};
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180), label);
    draw->AddText(pos, IM_COL32(120, 200, 255, 255), label);
}

void ViewportPanel::DrawAxisIndicator(const Camera& camera, const ImVec2& imageMin,
                                      const ImVec2& imageSize) const {
    if (imageSize.x < 80.0f || imageSize.y < 80.0f) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (draw == nullptr) {
        return;
    }

    constexpr float kLen = 40.0f;
    constexpr float kMargin = 16.0f;
    constexpr float kTipRadius = 3.0f;
    const ImVec2 origin{imageMin.x + kMargin + kLen, imageMin.y + imageSize.y - kMargin - kLen};

    const glm::mat3 viewRot{camera.ViewMatrix()};

    struct AxisSeg {
        ImVec2 tip{};
        float depth = 0.0f;
        ImU32 color = 0;
        const char* label = "";
    };

    std::array<AxisSeg, 3> axes{{
        {{}, 0.0f, IM_COL32(232, 72, 72, 255), "X"},
        {{}, 0.0f, IM_COL32(96, 200, 96, 255), "Y"},
        {{}, 0.0f, IM_COL32(80, 140, 245, 255), "Z"},
    }};
    const glm::vec3 world[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    for (int i = 0; i < 3; ++i) {
        const glm::vec3 v = viewRot * world[i];
        axes[i].tip = ImVec2(origin.x + v.x * kLen, origin.y - v.y * kLen);
        axes[i].depth = v.z;
    }

    std::array<int, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return axes[a].depth > axes[b].depth; });

    draw->AddCircleFilled(origin, kLen * 0.55f, IM_COL32(12, 12, 14, 140), 32);

    for (int idx : order) {
        const AxisSeg& axis = axes[idx];
        draw->AddLine(origin, axis.tip, axis.color, 2.25f);
        draw->AddCircleFilled(axis.tip, kTipRadius, axis.color, 12);
        const ImVec2 labelPos{axis.tip.x + 4.0f, axis.tip.y - 10.0f};
        draw->AddText(labelPos, axis.color, axis.label);
    }
}

void ViewportPanel::DrawGrid(EditorContext& ctx) {
    if (!ctx.showGrid || ctx.renderer == nullptr || ctx.camera == nullptr ||
        (ctx.piePlaying && !ctx.pieNewWindow)) {
        return;
    }

    const float step = std::max(ctx.gridSize, 0.05f);
    const auto snapDown = [step](float v) { return std::floor(v / step) * step; };
    const auto lineColor = [](int worldIndex, const glm::vec3& axisColor) {
        constexpr glm::vec3 kMajor{0.35f, 0.35f, 0.38f};
        constexpr glm::vec3 kMinor{0.22f, 0.22f, 0.24f};
        if (worldIndex == 0) {
            return axisColor;
        }
        if (worldIndex % 5 == 0) {
            return kMajor;
        }
        return kMinor;
    };

    const glm::vec3 eye = ctx.camera->GetCameraLocation();
    constexpr glm::vec3 kAxisX{0.55f, 0.2f, 0.2f};
    constexpr glm::vec3 kAxisY{0.2f, 0.55f, 0.25f};
    constexpr glm::vec3 kAxisZ{0.2f, 0.35f, 0.55f};

    int half = 40;
    if (ctx.viewMode != EEditorViewMode::Perspective) {
        half =
            std::clamp(static_cast<int>(std::ceil(ctx.camera->OrthoHeight() / step)) + 6, 12, 80);
    } else {
        const float reach = std::max({std::abs(eye.y) * 2.5f, 20.0f * step, 16.0f});
        half = std::clamp(static_cast<int>(std::ceil(reach / step)) + 4, 16, 80);
    }
    const float extent = static_cast<float>(half) * step;

    Renderer& r = *ctx.renderer;
    switch (ctx.viewMode) {
    case EEditorViewMode::OrthoFront: {
        const float z = snapDown(eye.z);
        const float ox = snapDown(eye.x);
        const float oy = snapDown(eye.y);
        for (int i = -half; i <= half; ++i) {
            const float y = oy + static_cast<float>(i) * step;
            const int yi = static_cast<int>(std::lround(y / step));
            r.AddDebugLine({ox - extent, y, z}, {ox + extent, y, z}, lineColor(yi, kAxisX));
            const float x = ox + static_cast<float>(i) * step;
            const int xi = static_cast<int>(std::lround(x / step));
            r.AddDebugLine({x, oy - extent, z}, {x, oy + extent, z}, lineColor(xi, kAxisY));
        }
        break;
    }
    case EEditorViewMode::OrthoSide: {
        const float x = snapDown(eye.x);
        const float oy = snapDown(eye.y);
        const float oz = snapDown(eye.z);
        for (int i = -half; i <= half; ++i) {
            const float y = oy + static_cast<float>(i) * step;
            const int yi = static_cast<int>(std::lround(y / step));
            r.AddDebugLine({x, y, oz - extent}, {x, y, oz + extent}, lineColor(yi, kAxisZ));
            const float z = oz + static_cast<float>(i) * step;
            const int zi = static_cast<int>(std::lround(z / step));
            r.AddDebugLine({x, oy - extent, z}, {x, oy + extent, z}, lineColor(zi, kAxisY));
        }
        break;
    }
    case EEditorViewMode::OrthoTop:
    case EEditorViewMode::Perspective:
    default: {
        const float ox = snapDown(eye.x);
        const float oz = snapDown(eye.z);
        constexpr float kY = 0.0f;
        for (int i = -half; i <= half; ++i) {
            const float z = oz + static_cast<float>(i) * step;
            const int zi = static_cast<int>(std::lround(z / step));
            r.AddDebugLine({ox - extent, kY, z}, {ox + extent, kY, z}, lineColor(zi, kAxisZ));
            const float x = ox + static_cast<float>(i) * step;
            const int xi = static_cast<int>(std::lround(x / step));
            r.AddDebugLine({x, kY, oz - extent}, {x, kY, oz + extent}, lineColor(xi, kAxisX));
        }
        break;
    }
    }
}

void ViewportPanel::DrawStatsOverlay(EditorContext& ctx, const ImVec2& imageMin) {
    const bool showStats = ctx.engine != nullptr ? ctx.engine->IsHudStatsVisible() : ctx.showStats;
    if (!showStats) {
        statsAccumTime_ = 0.0f;
        statsAccumFrames_ = 0;
        return;
    }

    statsAccumTime_ += ctx.deltaTime;
    ++statsAccumFrames_;
    if (statsAccumTime_ >= 0.25f && statsAccumFrames_ > 0) {
        displayMs_ = (statsAccumTime_ / static_cast<float>(statsAccumFrames_)) * 1000.0f;
        displayFps_ = static_cast<float>(statsAccumFrames_) / statsAccumTime_;
        statsAccumTime_ = 0.0f;
        statsAccumFrames_ = 0;
    }

    const FrameStats stats = ctx.renderer != nullptr ? ctx.renderer->GetFrameStats() : FrameStats{};
    const MemorySnapshot mem = queryMemorySnapshot();
    const int viewportW = ctx.viewportWidth > 0 ? ctx.viewportWidth : 1;
    const int viewportH = ctx.viewportHeight > 0 ? ctx.viewportHeight : 1;

    std::array<char, 512> line{};
    FormatDebugStatsText(line.data(), line.size(), displayFps_, displayMs_, mem, stats, viewportW,
                         viewportH);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float yOff =
        ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision ? 28.0f : 0.0f;
    const ImVec2 pos{imageMin.x + 10.0f, imageMin.y + 10.0f + yOff};
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180), line.data());
    draw->AddText(pos, IM_COL32(240, 240, 240, 255), line.data());
}

} // namespace leon::editor
