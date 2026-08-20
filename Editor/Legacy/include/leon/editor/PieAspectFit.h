#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace leon::editor {

/// How Selected Viewport / New Window PIE maps into the available panel or OS window.
enum class EEditorPieAspect : std::uint8_t {
    Stretch = 0, /// Fill the panel (may warp if panel ≠ game aspect).
    Fit16x9 = 1, /// Default — 1080p family; letterbox / pillarbox.
    Fit16x10 = 2,
    Fit4x3 = 3,
    Fit1x1 = 4,
    Fit9x16 = 5, /// Portrait / mobile.
};

[[nodiscard]] inline const char* PieAspectLabel(EEditorPieAspect mode) {
    switch (mode) {
    case EEditorPieAspect::Stretch:
        return "Stretch (Fill)";
    case EEditorPieAspect::Fit16x10:
        return "16:10";
    case EEditorPieAspect::Fit4x3:
        return "4:3";
    case EEditorPieAspect::Fit1x1:
        return "1:1";
    case EEditorPieAspect::Fit9x16:
        return "9:16";
    case EEditorPieAspect::Fit16x9:
    default:
        return "16:9";
    }
}

/// Target width/height ratio, or 0 for Stretch.
[[nodiscard]] inline float PieAspectRatio(EEditorPieAspect mode) {
    switch (mode) {
    case EEditorPieAspect::Stretch:
        return 0.0f;
    case EEditorPieAspect::Fit16x10:
        return 16.0f / 10.0f;
    case EEditorPieAspect::Fit4x3:
        return 4.0f / 3.0f;
    case EEditorPieAspect::Fit1x1:
        return 1.0f;
    case EEditorPieAspect::Fit9x16:
        return 9.0f / 16.0f;
    case EEditorPieAspect::Fit16x9:
    default:
        return 16.0f / 9.0f;
    }
}

[[nodiscard]] inline EEditorPieAspect PieAspectFromInt(int v) {
    switch (v) {
    case static_cast<int>(EEditorPieAspect::Stretch):
        return EEditorPieAspect::Stretch;
    case static_cast<int>(EEditorPieAspect::Fit16x10):
        return EEditorPieAspect::Fit16x10;
    case static_cast<int>(EEditorPieAspect::Fit4x3):
        return EEditorPieAspect::Fit4x3;
    case static_cast<int>(EEditorPieAspect::Fit1x1):
        return EEditorPieAspect::Fit1x1;
    case static_cast<int>(EEditorPieAspect::Fit9x16):
        return EEditorPieAspect::Fit9x16;
    case static_cast<int>(EEditorPieAspect::Fit16x9):
    default:
        return EEditorPieAspect::Fit16x9;
    }
}

/// Largest rect of `mode` aspect that fits inside panelW×panelH (top-left origin).
inline void FitPieViewport(int panelW, int panelH, EEditorPieAspect mode, int& outX, int& outY,
                           int& outW, int& outH) {
    panelW = std::max(1, panelW);
    panelH = std::max(1, panelH);
    const float target = PieAspectRatio(mode);
    if (target <= 0.0f) {
        outX = 0;
        outY = 0;
        outW = panelW;
        outH = panelH;
        return;
    }

    const float panelAspect = static_cast<float>(panelW) / static_cast<float>(panelH);
    if (panelAspect > target) {
        // Panel wider than game → pillarbox.
        outH = panelH;
        outW = std::max(1, static_cast<int>(std::lround(static_cast<float>(panelH) * target)));
        outX = (panelW - outW) / 2;
        outY = 0;
    } else {
        // Panel taller than game → letterbox.
        outW = panelW;
        outH = std::max(1, static_cast<int>(std::lround(static_cast<float>(panelW) / target)));
        outX = 0;
        outY = (panelH - outH) / 2;
    }
}

} // namespace leon::editor
