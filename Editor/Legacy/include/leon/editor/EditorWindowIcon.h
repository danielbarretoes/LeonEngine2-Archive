#pragma once

#include <iostream>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <string>

namespace leon::editor {

/// Load LeonEditor.png from engine assets onto a GLFW window (main editor or PIE New Window).
inline void ApplyLeonWindowIcon(Window& window) {
    const std::string iconPath = ResolveAssetPath("assets/Icons/LeonEditor.png");
    if (iconPath.empty()) {
        return;
    }
    if (!window.SetIconFromFile(iconPath.c_str())) {
        std::cerr << "Editor: failed to load window icon: " << iconPath << '\n';
    }
}

} // namespace leon::editor
