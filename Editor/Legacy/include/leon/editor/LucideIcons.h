#pragma once

#include <cstdint>
#include <imgui.h>

namespace leon::editor {

/// Lucide icon ids used by Content Browser / Outliner (stroke icons, 24×24 viewBox).
enum class ELucideIcon : std::uint8_t {
    Folder = 0,
    FolderOpen,
    Box,
    Boxes,
    Map,
    FileCode,
    Image,
    Layers,
    PersonStanding,
    Bone,
    Film,
    Clapperboard,
    Activity,
    Atom,
    Sun,
    FileText,
    Hexagon,
    Package,
    Braces,
    Component,
    Circle,
    Zap,
    Flame,
    Bot,
    Globe,
    Lightbulb,
    User,
    LayoutGrid,
    Waypoints,
    Crosshair,
    Skull,
    Swords,
    Magnet,
    Unplug,
    Cpu,
    FileBox,
    Eye,
    EyeOff,
    Lock,
    LockOpen,
    Play,
    Pause,
    Square,
    Hammer,
    RefreshCw,
    FolderPlus,
    Save,
    Settings,
    Search,
    Plus,
    Count
};

/// Stroke a Lucide icon into `draw` inside [min,max] (aspect-fit, centered).
void DrawLucideIcon(ImDrawList* draw, ImVec2 min, ImVec2 max, ELucideIcon icon, ImU32 color,
                    float strokeWidth = 0.0f);

/// InvisibleButton + Lucide glyph (toolbar / chips). Returns true when clicked.
[[nodiscard]] bool LucideIconButton(ELucideIcon icon, const char* id, ImVec2 size,
                                    ImU32 color = IM_COL32(230, 230, 235, 255),
                                    ImU32 bgHovered = IM_COL32(70, 74, 82, 255));

/// Content Browser tile / tree kind → Lucide glyph.
[[nodiscard]] ELucideIcon LucideIconForContentKind(int kindOrdinal);

} // namespace leon::editor
