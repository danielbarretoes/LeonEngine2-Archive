#include "Editor/UI/FLucideIcons.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// Include the vector path definitions
#include "../../Legacy/include/leon/editor/LucideIconData.inl"

namespace Leon::Editor {

    namespace {

        const char* LucideName(ELucideIcon icon) {
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

            bool Has() {
                SkipWs();
                return cur < end;
            }

            bool PeekCmd(char& out) {
                SkipWs();
                if (cur >= end)
                    return false;
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
                if (prev == 'M')
                    return 'L';
                if (prev == 'm')
                    return 'l';
                return prev;
            }

            float Num() {
                SkipWs();
                char* next = nullptr;
                const float v = std::strtof(cur, &next);
                if (next == cur)
                    return 0.0f;
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

        void AppendArc(ImDrawList* draw, ImVec2 p0, float rx, float ry, float xAxisRotDeg, bool large, bool sweep,
                       ImVec2 p1) {
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
            if (large == sweep)
                cFactor = -cFactor;

            const float cxp = cFactor * (rx * y1p) / ry;
            const float cyp = cFactor * (-ry * x1p) / rx;
            const float cx = cosPhi * cxp - sinPhi * cyp + (p0.x + p1.x) * 0.5f;
            const float cy = sinPhi * cxp + cosPhi * cyp + (p0.y + p1.y) * 0.5f;

            auto angle = [](float ux, float uy, float vx, float vy) {
                const float n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
                if (n < 1.0e-8f)
                    return 0.0f;
                float a = std::acos(std::clamp((ux * vx + uy * vy) / n, -1.0f, 1.0f));
                if (ux * vy - uy * vx < 0.0f)
                    a = -a;
                return a;
            };

            const float theta1 = angle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
            float dTheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
            if (!sweep && dTheta > 0.0f)
                dTheta -= 2.0f * kPi;
            else if (sweep && dTheta < 0.0f)
                dTheta += 2.0f * kPi;

            const int segments = std::clamp(static_cast<int>(std::ceil(std::abs(dTheta) / (kPi * 0.25f))) * 4, 4, 32);
            for (int i = 1; i <= segments; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(segments);
                const float th = theta1 + dTheta * t;
                const float cosTh = std::cos(th);
                const float sinTh = std::sin(th);
                const float px = cosPhi * rx * cosTh - sinPhi * ry * sinTh + cx;
                const float py = sinPhi * rx * cosTh + cosPhi * ry * sinTh + cy;
                draw->PathLineTo(ImVec2(px, py));
            }
        }

        void StrokeSvgPath(ImDrawList* draw, const char* pathStr, ImVec2 origin, float scale, ImU32 color,
                           float thickness) {
            PathParser p(pathStr);
            ImVec2 curPos{0.0f, 0.0f};
            ImVec2 subStart{0.0f, 0.0f};
            char prevCmd = 0;
            bool inPath = false;

            auto toScreen = [&](ImVec2 v) -> ImVec2 { return {origin.x + v.x * scale, origin.y + v.y * scale}; };

            auto flush = [&]() {
                if (inPath) {
                    draw->PathStroke(color, 0, thickness);
                    inPath = false;
                }
            };

            while (p.Has()) {
                char cmd = p.TakeCmd(prevCmd);
                prevCmd = cmd;

                switch (cmd) {
                case 'M':
                case 'm': {
                    flush();
                    float x = p.Num();
                    float y = p.Num();
                    if (cmd == 'm') {
                        curPos.x += x;
                        curPos.y += y;
                    } else {
                        curPos.x = x;
                        curPos.y = y;
                    }
                    subStart = curPos;
                    draw->PathLineTo(toScreen(curPos));
                    inPath = true;
                    break;
                }
                case 'L':
                case 'l': {
                    float x = p.Num();
                    float y = p.Num();
                    if (cmd == 'l') {
                        curPos.x += x;
                        curPos.y += y;
                    } else {
                        curPos.x = x;
                        curPos.y = y;
                    }
                    draw->PathLineTo(toScreen(curPos));
                    inPath = true;
                    break;
                }
                case 'H':
                case 'h': {
                    float x = p.Num();
                    if (cmd == 'h')
                        curPos.x += x;
                    else
                        curPos.x = x;
                    draw->PathLineTo(toScreen(curPos));
                    inPath = true;
                    break;
                }
                case 'V':
                case 'v': {
                    float y = p.Num();
                    if (cmd == 'v')
                        curPos.y += y;
                    else
                        curPos.y = y;
                    draw->PathLineTo(toScreen(curPos));
                    inPath = true;
                    break;
                }
                case 'C':
                case 'c': {
                    float x1 = p.Num(), y1 = p.Num();
                    float x2 = p.Num(), y2 = p.Num();
                    float x = p.Num(), y = p.Num();
                    ImVec2 p0 = toScreen(curPos);
                    ImVec2 p1 = toScreen(cmd == 'c' ? ImVec2{curPos.x + x1, curPos.y + y1} : ImVec2{x1, y1});
                    ImVec2 p2 = toScreen(cmd == 'c' ? ImVec2{curPos.x + x2, curPos.y + y2} : ImVec2{x2, y2});
                    if (cmd == 'c') {
                        curPos.x += x;
                        curPos.y += y;
                    } else {
                        curPos.x = x;
                        curPos.y = y;
                    }
                    ImVec2 p3 = toScreen(curPos);
                    AppendCubic(draw, p0, p1, p2, p3);
                    inPath = true;
                    break;
                }
                case 'A':
                case 'a': {
                    float rx = p.Num();
                    float ry = p.Num();
                    float xRot = p.Num();
                    bool large = p.Num() != 0.0f;
                    bool sweep = p.Num() != 0.0f;
                    float x = p.Num();
                    float y = p.Num();
                    ImVec2 p0 = toScreen(curPos);
                    if (cmd == 'a') {
                        curPos.x += x;
                        curPos.y += y;
                    } else {
                        curPos.x = x;
                        curPos.y = y;
                    }
                    ImVec2 p1 = toScreen(curPos);
                    AppendArc(draw, p0, rx * scale, ry * scale, xRot, large, sweep, p1);
                    inPath = true;
                    break;
                }
                case 'Z':
                case 'z': {
                    curPos = subStart;
                    if (inPath) {
                        draw->PathStroke(color, ImDrawFlags_Closed, thickness);
                        inPath = false;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            flush();
        }

    } // namespace

    void FLucideIcons::DrawIcon(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax, ELucideIcon InIcon, ImU32 InColor,
                                float InStrokeWidth) {
        if (!InDrawList)
            return;
        const float w = InMax.x - InMin.x;
        const float h = InMax.y - InMin.y;
        if (w < 4.0f || h < 4.0f)
            return;

        constexpr float kView = 24.0f;
        constexpr float kPad = 0.12f;
        const float inner = std::min(w, h) * (1.0f - 2.0f * kPad);
        const float scale = inner / kView;
        const ImVec2 origin{InMin.x + (w - inner) * 0.5f, InMin.y + (h - inner) * 0.5f};
        const float thickness = (InStrokeWidth > 0.0f) ? InStrokeWidth : std::max(1.35f, 2.0f * scale);

        const leon::editor::lucide_data::IconPaths paths = leon::editor::lucide_data::PathsFor(LucideName(InIcon));
        if (paths.paths == nullptr || paths.count <= 0) {
            InDrawList->AddCircle(ImVec2((InMin.x + InMax.x) * 0.5f, (InMin.y + InMax.y) * 0.5f), inner * 0.35f,
                                  InColor, 0, thickness);
            return;
        }

        const int vtxBefore = InDrawList->VtxBuffer.Size;
        for (int i = 0; i < paths.count; ++i) {
            StrokeSvgPath(InDrawList, paths.paths[i], origin, scale, InColor, thickness);
        }

        if (InDrawList->VtxBuffer.Size <= vtxBefore) {
            const ImVec2 c{(InMin.x + InMax.x) * 0.5f, (InMin.y + InMax.y) * 0.5f};
            InDrawList->AddCircle(c, inner * 0.32f, InColor, 0, thickness);
        }
    }

    bool FLucideIcons::IconButton(ELucideIcon InIcon, const char* InId, ImVec2 InSize, ImU32 InColor,
                                  ImU32 InBgHovered) {
        ImGui::PushID(InId);
        const bool clicked = ImGui::InvisibleButton("##lucide_btn", InSize);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (ImGui::IsItemHovered()) {
            draw->AddRectFilled(min, max, InBgHovered, 3.0f);
        }
        if (ImGui::IsItemActive()) {
            draw->AddRectFilled(min, max, IM_COL32(55, 58, 66, 255), 3.0f);
        }
        DrawIcon(draw, min, max, InIcon, InColor);
        ImGui::PopID();
        return clicked;
    }

} // namespace Leon::Editor
