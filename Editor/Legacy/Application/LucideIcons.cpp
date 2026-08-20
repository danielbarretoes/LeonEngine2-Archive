#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <leon/editor/LucideIconData.inl>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace {

[[nodiscard]] const char* LucideName(ELucideIcon icon) {
    switch (icon) {
    case ELucideIcon::Folder:
        return "folder";
    case ELucideIcon::FolderOpen:
        return "folder-open";
    case ELucideIcon::Box:
        return "box";
    case ELucideIcon::Boxes:
        return "boxes";
    case ELucideIcon::Map:
        return "map";
    case ELucideIcon::FileCode:
        return "file-code";
    case ELucideIcon::Image:
        return "image";
    case ELucideIcon::Layers:
        return "layers";
    case ELucideIcon::PersonStanding:
        return "person-standing";
    case ELucideIcon::Bone:
        return "bone";
    case ELucideIcon::Film:
        return "film";
    case ELucideIcon::Clapperboard:
        return "clapperboard";
    case ELucideIcon::Activity:
        return "activity";
    case ELucideIcon::Atom:
        return "atom";
    case ELucideIcon::Sun:
        return "sun";
    case ELucideIcon::FileText:
        return "file-text";
    case ELucideIcon::Hexagon:
        return "hexagon";
    case ELucideIcon::Package:
        return "package";
    case ELucideIcon::Braces:
        return "braces";
    case ELucideIcon::Component:
        return "component";
    case ELucideIcon::Circle:
        return "circle";
    case ELucideIcon::Zap:
        return "zap";
    case ELucideIcon::Flame:
        return "flame";
    case ELucideIcon::Bot:
        return "bot";
    case ELucideIcon::Globe:
        return "globe";
    case ELucideIcon::Lightbulb:
        return "lightbulb";
    case ELucideIcon::User:
        return "user";
    case ELucideIcon::LayoutGrid:
        return "layout-grid";
    case ELucideIcon::Waypoints:
        return "waypoints";
    case ELucideIcon::Crosshair:
        return "crosshair";
    case ELucideIcon::Skull:
        return "skull";
    case ELucideIcon::Swords:
        return "swords";
    case ELucideIcon::Magnet:
        return "magnet";
    case ELucideIcon::Unplug:
        return "unplug";
    case ELucideIcon::Cpu:
        return "cpu";
    case ELucideIcon::FileBox:
        return "file-box";
    case ELucideIcon::Eye:
        return "eye";
    case ELucideIcon::EyeOff:
        return "eye-off";
    case ELucideIcon::Lock:
        return "lock";
    case ELucideIcon::LockOpen:
        return "lock-open";
    case ELucideIcon::Play:
        return "play";
    case ELucideIcon::Pause:
        return "pause";
    case ELucideIcon::Square:
        return "square";
    case ELucideIcon::Hammer:
        return "hammer";
    case ELucideIcon::RefreshCw:
        return "refresh-cw";
    case ELucideIcon::FolderPlus:
        return "folder-plus";
    case ELucideIcon::Save:
        return "save";
    case ELucideIcon::Settings:
        return "settings";
    case ELucideIcon::Search:
        return "search";
    case ELucideIcon::Plus:
        return "plus";
    case ELucideIcon::Count:
        break;
    }
    return "circle";
}

struct PathParser {
    const char* cur = nullptr;
    const char* end = nullptr;

    explicit PathParser(const char* d) : cur(d), end(d + std::strlen(d)) {}

    void SkipWs() {
        while (cur < end && (std::isspace(static_cast<unsigned char>(*cur)) || *cur == ',')) {
            ++cur;
        }
    }

    [[nodiscard]] bool Has() {
        SkipWs();
        return cur < end;
    }

    [[nodiscard]] bool PeekCmd(char& out) {
        SkipWs();
        if (cur >= end) {
            return false;
        }
        if (std::isalpha(static_cast<unsigned char>(*cur))) {
            out = *cur;
            return true;
        }
        return false;
    }

    char TakeCmd(char prev) {
        char c = 0;
        if (PeekCmd(c)) {
            ++cur;
            return c;
        }
        // Implicit repeat of previous command (SVG).
        if (prev == 'M') {
            return 'L';
        }
        if (prev == 'm') {
            return 'l';
        }
        return prev;
    }

    [[nodiscard]] float Num() {
        SkipWs();
        char* next = nullptr;
        const float v = std::strtof(cur, &next);
        if (next == cur) {
            return 0.0f;
        }
        cur = next;
        return v;
    }
};

void AppendCubic(ImDrawList* draw, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, int steps = 8) {
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float u = 1.0f - t;
        const float uu = u * u;
        const float tt = t * t;
        const ImVec2 p{uu * u * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + tt * t * p3.x,
                       uu * u * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + tt * t * p3.y};
        draw->PathLineTo(p);
    }
}

void AppendQuad(ImDrawList* draw, ImVec2 p0, ImVec2 p1, ImVec2 p2, int steps = 8) {
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float u = 1.0f - t;
        const ImVec2 p{u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x,
                       u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y};
        draw->PathLineTo(p);
    }
}

/// SVG elliptical arc → polyline samples (W3C endpoint-to-center parameterization).
void AppendArc(ImDrawList* draw, ImVec2 p0, float rx, float ry, float xAxisRotDeg, bool large,
               bool sweep, ImVec2 p1) {
    if (rx <= 1.0e-4f || ry <= 1.0e-4f ||
        (std::abs(p0.x - p1.x) < 1.0e-4f && std::abs(p0.y - p1.y) < 1.0e-4f)) {
        draw->PathLineTo(p1);
        return;
    }
    constexpr float kPi = 3.14159265358979323846f;
    const float phi = xAxisRotDeg * (kPi / 180.0f);
    const float cosPhi = std::cos(phi);
    const float sinPhi = std::sin(phi);

    const float dx = (p0.x - p1.x) * 0.5f;
    const float dy = (p0.y - p1.y) * 0.5f;
    float x1p = cosPhi * dx + sinPhi * dy;
    float y1p = -sinPhi * dx + cosPhi * dy;

    rx = std::abs(rx);
    ry = std::abs(ry);
    float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0f) {
        const float s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
    }

    const float rxSq = rx * rx;
    const float rySq = ry * ry;
    const float x1pSq = x1p * x1p;
    const float y1pSq = y1p * y1p;
    float num = rxSq * rySq - rxSq * y1pSq - rySq * x1pSq;
    float den = rxSq * y1pSq + rySq * x1pSq;
    float cFactor = (den > 1.0e-8f) ? std::sqrt(std::max(0.0f, num / den)) : 0.0f;
    if (large == sweep) {
        cFactor = -cFactor;
    }
    const float cxp = cFactor * (rx * y1p) / ry;
    const float cyp = cFactor * (-ry * x1p) / rx;
    const float cx = cosPhi * cxp - sinPhi * cyp + (p0.x + p1.x) * 0.5f;
    const float cy = sinPhi * cxp + cosPhi * cyp + (p0.y + p1.y) * 0.5f;

    auto angle = [](float ux, float uy, float vx, float vy) {
        const float n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        if (n < 1.0e-8f) {
            return 0.0f;
        }
        float a = std::acos(std::clamp((ux * vx + uy * vy) / n, -1.0f, 1.0f));
        if (ux * vy - uy * vx < 0.0f) {
            a = -a;
        }
        return a;
    };

    const float start = angle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float delta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && delta > 0.0f) {
        delta -= 2.0f * kPi;
    } else if (sweep && delta < 0.0f) {
        delta += 2.0f * kPi;
    }
    if (!std::isfinite(start) || !std::isfinite(delta) || !std::isfinite(cx) ||
        !std::isfinite(cy)) {
        draw->PathLineTo(p1);
        return;
    }

    const int segments = std::clamp(static_cast<int>(std::ceil(std::abs(delta) / 0.35f)), 3, 24);
    for (int i = 1; i <= segments; ++i) {
        const float a = start + delta * (static_cast<float>(i) / static_cast<float>(segments));
        const float cosA = std::cos(a);
        const float sinA = std::sin(a);
        draw->PathLineTo(ImVec2(cx + cosPhi * rx * cosA - sinPhi * ry * sinA,
                                cy + sinPhi * rx * cosA + cosPhi * ry * sinA));
    }
}

void StrokeSvgPath(ImDrawList* draw, const char* d, ImVec2 origin, float scale, ImU32 color,
                   float thickness) {
    PathParser p(d);
    ImVec2 cur{0, 0};
    ImVec2 start{0, 0};
    ImVec2 lastC{0, 0};
    ImVec2 lastQ{0, 0};
    char prev = 0;
    bool pathOpen = false;

    auto map = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    auto flush = [&]() {
        if (pathOpen) {
            draw->PathStroke(color, 0, thickness);
            pathOpen = false;
        }
    };
    auto ensure = [&]() {
        if (!pathOpen) {
            draw->PathClear();
            draw->PathLineTo(map(cur.x, cur.y));
            pathOpen = true;
        }
    };

    while (p.Has()) {
        const char cmd = p.TakeCmd(prev);
        prev = cmd;
        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char op = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

        if (op == 'M') {
            flush();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x += cur.x;
                y += cur.y;
            }
            cur = {x, y};
            start = cur;
            draw->PathClear();
            draw->PathLineTo(map(cur.x, cur.y));
            pathOpen = true;
            lastC = cur;
            lastQ = cur;
            // Subsequent pairs are implicit L/l.
            while (p.Has()) {
                char peek = 0;
                if (p.PeekCmd(peek)) {
                    break;
                }
                float nx = p.Num();
                float ny = p.Num();
                if (rel) {
                    nx += cur.x;
                    ny += cur.y;
                }
                cur = {nx, ny};
                draw->PathLineTo(map(cur.x, cur.y));
                lastC = cur;
                lastQ = cur;
            }
            continue;
        }

        if (op == 'Z') {
            ensure();
            draw->PathLineTo(map(start.x, start.y));
            cur = start;
            flush();
            continue;
        }

        if (op == 'L') {
            ensure();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x += cur.x;
                y += cur.y;
            }
            cur = {x, y};
            draw->PathLineTo(map(cur.x, cur.y));
            lastC = cur;
            lastQ = cur;
            continue;
        }

        if (op == 'H') {
            ensure();
            float x = p.Num();
            if (rel) {
                x += cur.x;
            }
            cur.x = x;
            draw->PathLineTo(map(cur.x, cur.y));
            lastC = cur;
            lastQ = cur;
            continue;
        }

        if (op == 'V') {
            ensure();
            float y = p.Num();
            if (rel) {
                y += cur.y;
            }
            cur.y = y;
            draw->PathLineTo(map(cur.x, cur.y));
            lastC = cur;
            lastQ = cur;
            continue;
        }

        if (op == 'C') {
            ensure();
            float x1 = p.Num();
            float y1 = p.Num();
            float x2 = p.Num();
            float y2 = p.Num();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x1 += cur.x;
                y1 += cur.y;
                x2 += cur.x;
                y2 += cur.y;
                x += cur.x;
                y += cur.y;
            }
            const ImVec2 p0 = map(cur.x, cur.y);
            AppendCubic(draw, p0, map(x1, y1), map(x2, y2), map(x, y));
            lastC = {x2, y2};
            lastQ = {x, y};
            cur = {x, y};
            continue;
        }

        if (op == 'S') {
            ensure();
            float x2 = p.Num();
            float y2 = p.Num();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x2 += cur.x;
                y2 += cur.y;
                x += cur.x;
                y += cur.y;
            }
            const float x1 = 2.0f * cur.x - lastC.x;
            const float y1 = 2.0f * cur.y - lastC.y;
            const ImVec2 p0 = map(cur.x, cur.y);
            AppendCubic(draw, p0, map(x1, y1), map(x2, y2), map(x, y));
            lastC = {x2, y2};
            lastQ = {x, y};
            cur = {x, y};
            continue;
        }

        if (op == 'Q') {
            ensure();
            float x1 = p.Num();
            float y1 = p.Num();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x1 += cur.x;
                y1 += cur.y;
                x += cur.x;
                y += cur.y;
            }
            const ImVec2 p0 = map(cur.x, cur.y);
            AppendQuad(draw, p0, map(x1, y1), map(x, y));
            lastQ = {x1, y1};
            lastC = cur;
            cur = {x, y};
            continue;
        }

        if (op == 'T') {
            ensure();
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x += cur.x;
                y += cur.y;
            }
            const float x1 = 2.0f * cur.x - lastQ.x;
            const float y1 = 2.0f * cur.y - lastQ.y;
            const ImVec2 p0 = map(cur.x, cur.y);
            AppendQuad(draw, p0, map(x1, y1), map(x, y));
            lastQ = {x1, y1};
            lastC = cur;
            cur = {x, y};
            continue;
        }

        if (op == 'A') {
            ensure();
            float rx = p.Num();
            float ry = p.Num();
            float rot = p.Num();
            const bool large = p.Num() != 0.0f;
            const bool sweep = p.Num() != 0.0f;
            float x = p.Num();
            float y = p.Num();
            if (rel) {
                x += cur.x;
                y += cur.y;
            }
            AppendArc(draw, map(cur.x, cur.y), rx * scale, ry * scale, rot, large, sweep,
                      map(x, y));
            // AppendArc already maps; fix last point tracking in icon space.
            cur = {x, y};
            lastC = cur;
            lastQ = cur;
            continue;
        }

        // Unknown / exhausted numbers — bail to avoid infinite loop.
        break;
    }
    flush();
}

} // namespace

void DrawLucideIcon(ImDrawList* draw, ImVec2 min, ImVec2 max, ELucideIcon icon, ImU32 color,
                    float strokeWidth) {
    if (draw == nullptr) {
        return;
    }
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    if (w < 4.0f || h < 4.0f) {
        return;
    }

    constexpr float kView = 24.0f;
    constexpr float kPad = 0.12f; // keep stroke inside tile
    const float inner = std::min(w, h) * (1.0f - 2.0f * kPad);
    const float scale = inner / kView;
    const ImVec2 origin{min.x + (w - inner) * 0.5f, min.y + (h - inner) * 0.5f};
    const float thickness = (strokeWidth > 0.0f) ? strokeWidth : std::max(1.35f, 2.0f * scale);

    const lucide_data::IconPaths paths = lucide_data::PathsFor(LucideName(icon));
    if (paths.paths == nullptr || paths.count <= 0) {
        draw->AddCircle(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), inner * 0.35f,
                        color, 0, thickness);
        return;
    }
    const int vtxBefore = draw->VtxBuffer.Size;
    for (int i = 0; i < paths.count; ++i) {
        StrokeSvgPath(draw, paths.paths[i], origin, scale, color, thickness);
    }
    // If the stroker produced nothing (degenerate arcs), fall back to a simple glyph.
    if (draw->VtxBuffer.Size <= vtxBefore) {
        const ImVec2 c{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
        if (icon == ELucideIcon::Folder || icon == ELucideIcon::FolderOpen) {
            const float s = inner;
            const ImVec2 p{c.x - s * 0.5f, c.y - s * 0.5f};
            draw->AddRectFilled(ImVec2(p.x + s * 0.12f, p.y + s * 0.38f),
                                ImVec2(p.x + s * 0.88f, p.y + s * 0.82f), color, 2.0f);
            draw->AddRectFilled(ImVec2(p.x + s * 0.12f, p.y + s * 0.28f),
                                ImVec2(p.x + s * 0.48f, p.y + s * 0.42f), color, 2.0f);
        } else {
            draw->AddCircle(c, inner * 0.32f, color, 0, thickness);
        }
    }
}

bool LucideIconButton(ELucideIcon icon, const char* id, ImVec2 size, ImU32 color, ImU32 bgHovered) {
    ImGui::PushID(id);
    const bool clicked = ImGui::InvisibleButton("##lucide_btn", size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered()) {
        draw->AddRectFilled(min, max, bgHovered, 3.0f);
    }
    if (ImGui::IsItemActive()) {
        draw->AddRectFilled(min, max, IM_COL32(55, 58, 66, 255), 3.0f);
    }
    DrawLucideIcon(draw, min, max, icon, color);
    ImGui::PopID();
    return clicked;
}

ELucideIcon LucideIconForContentKind(int kindOrdinal) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    switch (static_cast<Kind>(kindOrdinal)) {
    case Kind::Folder:
        return ELucideIcon::Folder;
    case Kind::Level:
        return ELucideIcon::Map;
    case Kind::Material:
        return ELucideIcon::Layers;
    case Kind::MaterialGraph:
        return ELucideIcon::Atom;
    case Kind::StaticMesh:
        return ELucideIcon::Box;
    case Kind::Texture:
        return ELucideIcon::Image;
    case Kind::Blueprint:
        return ELucideIcon::Braces;
    case Kind::UserWidget:
        return ELucideIcon::LayoutGrid;
    case Kind::EnginePrimitive:
        return ELucideIcon::Box;
    case Kind::FbxSource:
        return ELucideIcon::Package;
    case Kind::Character:
        return ELucideIcon::PersonStanding;
    case Kind::SkelMesh:
        return ELucideIcon::Bone;
    case Kind::Skeleton:
        return ELucideIcon::Bone;
    case Kind::Anim:
        return ELucideIcon::Film;
    case Kind::AnimMontage:
        return ELucideIcon::Clapperboard;
    case Kind::AnimBlueprint:
        return ELucideIcon::Activity;
    case Kind::PhysicsAsset:
        return ELucideIcon::Hexagon;
    case Kind::Hdr:
        return ELucideIcon::Sun;
    case Kind::Lightmap:
        return ELucideIcon::FileBox;
    default:
        return ELucideIcon::FileText;
    }
}

} // namespace leon::editor
