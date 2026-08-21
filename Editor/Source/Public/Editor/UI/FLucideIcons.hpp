#pragma once

#include "Core/Base.hpp"
#include <cstdint>
#include <imgui.h>

namespace Leon::Editor {

    enum class ELucideIcon : uint8_t {
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
        ChevronLeft,
        ChevronRight,
        ChevronDown,
        Download,
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
        MousePointer,
        Move,
        Scaling,
        X,
        Copy,
        Trash,
        Pencil,
        Check,
        Count
    };

    /**
     * @brief Vector-based Lucide icon drawing onto ImDrawList.
     */
    class FLucideIcons {
    public:
        static void DrawIcon(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax, ELucideIcon InIcon, ImU32 InColor,
                             float InStrokeWidth = 0.0f);

        static bool IconButton(ELucideIcon InIcon, const char* InId, ImVec2 InSize,
                               ImU32 InColor = IM_COL32(230, 230, 235, 255),
                               ImU32 InBgHovered = IM_COL32(70, 74, 82, 255));
    };

} // namespace Leon::Editor
