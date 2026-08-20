#include <algorithm>
#include <fstream>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorPreferences.h>
#include <leon/net/NetProtocol.h>
#include <nlohmann/json.hpp>

namespace leon::editor {
namespace {

[[nodiscard]] int ClampPlayers(int n) {
    return std::clamp(n, 1, leon::net::kMaxPlayers);
}

[[nodiscard]] EEditorPlayMode PlayModeFromInt(int v) {
    return v == static_cast<int>(EEditorPlayMode::NewEditorWindow)
               ? EEditorPlayMode::NewEditorWindow
               : EEditorPlayMode::SelectedViewport;
}

[[nodiscard]] EEditorPlayNetMode NetModeFromInt(int v) {
    switch (v) {
    case static_cast<int>(EEditorPlayNetMode::ListenServer):
        return EEditorPlayNetMode::ListenServer;
    case static_cast<int>(EEditorPlayNetMode::Client):
        return EEditorPlayNetMode::Client;
    default:
        return EEditorPlayNetMode::Standalone;
    }
}

[[nodiscard]] EEditorViewportViewMode ViewportViewModeFromInt(int v) {
    switch (v) {
    case static_cast<int>(EEditorViewportViewMode::PlayerCollision):
        return EEditorViewportViewMode::PlayerCollision;
    case static_cast<int>(EEditorViewportViewMode::Wireframe):
        return EEditorViewportViewMode::Wireframe;
    default:
        return EEditorViewportViewMode::Lit;
    }
}

[[nodiscard]] EGizmoOperation GizmoOpFromInt(int v) {
    switch (v) {
    case static_cast<int>(EGizmoOperation::Rotate):
        return EGizmoOperation::Rotate;
    case static_cast<int>(EGizmoOperation::Scale):
        return EGizmoOperation::Scale;
    default:
        return EGizmoOperation::Translate;
    }
}

[[nodiscard]] EGizmoSpace GizmoSpaceFromInt(int v) {
    return v == static_cast<int>(EGizmoSpace::World) ? EGizmoSpace::World : EGizmoSpace::Local;
}

} // namespace

bool EditorUserPreferences::Equals(const EditorUserPreferences& other) const {
    return showGrid == other.showGrid && showStats == other.showStats &&
           snapEnabled == other.snapEnabled && surfaceSnapEnabled == other.surfaceSnapEnabled &&
           gridSize == other.gridSize && rotationSnapDegrees == other.rotationSnapDegrees &&
           playMode == other.playMode && pieNumberOfPlayers == other.pieNumberOfPlayers &&
           pieNetMode == other.pieNetMode && pieClientAddress == other.pieClientAddress &&
           pieAspect == other.pieAspect && viewportViewMode == other.viewportViewMode &&
           gizmoOp == other.gizmoOp && gizmoSpace == other.gizmoSpace &&
           contentFavoritePaths == other.contentFavoritePaths;
}

namespace EditorPreferences {

std::string FilePath() {
    return (ExecutableDirectory() / "editor_preferences.json").lexically_normal().string();
}

EditorUserPreferences Capture(const EditorContext& ctx) {
    EditorUserPreferences p;
    p.showGrid = ctx.showGrid;
    p.showStats = ctx.showStats;
    p.snapEnabled = ctx.snapEnabled;
    p.surfaceSnapEnabled = ctx.surfaceSnapEnabled;
    p.gridSize = ctx.gridSize;
    p.rotationSnapDegrees = ctx.rotationSnapDegrees;
    p.playMode = ctx.playMode;
    p.pieNumberOfPlayers = ClampPlayers(ctx.pieNumberOfPlayers);
    p.pieNetMode = ctx.pieNetMode;
    p.pieClientAddress = ctx.pieClientAddress.empty() ? "127.0.0.1" : ctx.pieClientAddress;
    p.pieAspect = ctx.pieAspect;
    p.viewportViewMode = ctx.viewportViewMode;
    p.gizmoOp = ctx.gizmoOp;
    p.gizmoSpace = ctx.gizmoSpace;
    p.contentFavoritePaths = ctx.contentFavoritePaths;
    return p;
}

void Apply(EditorContext& ctx, const EditorUserPreferences& prefs) {
    ctx.showGrid = prefs.showGrid;
    ctx.showStats = prefs.showStats;
    ctx.snapEnabled = prefs.snapEnabled;
    ctx.surfaceSnapEnabled = prefs.surfaceSnapEnabled;
    ctx.gridSize = std::clamp(prefs.gridSize, 0.05f, 50.0f);
    ctx.rotationSnapDegrees = std::clamp(prefs.rotationSnapDegrees, 1.0f, 90.0f);
    ctx.playMode = prefs.playMode;
    ctx.pieNumberOfPlayers = ClampPlayers(prefs.pieNumberOfPlayers);
    ctx.pieNetMode = prefs.pieNetMode;
    ctx.pieClientAddress = prefs.pieClientAddress.empty() ? "127.0.0.1" : prefs.pieClientAddress;
    ctx.pieAspect = prefs.pieAspect;
    ctx.viewportViewMode = prefs.viewportViewMode;
    ctx.gizmoOp = prefs.gizmoOp;
    ctx.gizmoSpace = prefs.gizmoSpace;
    ctx.contentFavoritePaths = prefs.contentFavoritePaths;
}

EditorUserPreferences Load() {
    EditorUserPreferences prefs;
    const std::string path = FilePath();
    std::ifstream in(path);
    if (!in.is_open()) {
        return prefs;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (...) {
        return prefs;
    }
    if (!doc.is_object()) {
        return prefs;
    }

    // Flow: editor_preferences.json → EditorContext user options
    // 1. Read optional fields with defaults
    // 2. Clamp enums / ranges
    prefs.showGrid = doc.value("showGrid", prefs.showGrid);
    prefs.showStats = doc.value("showStats", prefs.showStats);
    prefs.snapEnabled = doc.value("snapEnabled", prefs.snapEnabled);
    prefs.surfaceSnapEnabled = doc.value("surfaceSnapEnabled", prefs.surfaceSnapEnabled);
    prefs.gridSize = doc.value("gridSize", prefs.gridSize);
    prefs.rotationSnapDegrees = doc.value("rotationSnapDegrees", prefs.rotationSnapDegrees);
    prefs.playMode = PlayModeFromInt(doc.value("playMode", static_cast<int>(prefs.playMode)));
    prefs.pieNumberOfPlayers =
        ClampPlayers(doc.value("pieNumberOfPlayers", prefs.pieNumberOfPlayers));
    prefs.pieNetMode = NetModeFromInt(doc.value("pieNetMode", static_cast<int>(prefs.pieNetMode)));
    prefs.pieClientAddress = doc.value("pieClientAddress", prefs.pieClientAddress);
    prefs.pieAspect = PieAspectFromInt(doc.value("pieAspect", static_cast<int>(prefs.pieAspect)));
    prefs.viewportViewMode = ViewportViewModeFromInt(
        doc.value("viewportViewMode", static_cast<int>(prefs.viewportViewMode)));
    prefs.gizmoOp = GizmoOpFromInt(doc.value("gizmoOp", static_cast<int>(prefs.gizmoOp)));
    prefs.gizmoSpace =
        GizmoSpaceFromInt(doc.value("gizmoSpace", static_cast<int>(prefs.gizmoSpace)));
    if (doc.contains("contentFavorites") && doc["contentFavorites"].is_array()) {
        prefs.contentFavoritePaths.clear();
        for (const auto& entry : doc["contentFavorites"]) {
            if (entry.is_string()) {
                prefs.contentFavoritePaths.push_back(entry.get<std::string>());
            }
        }
    }
    return prefs;
}

void Save(const EditorUserPreferences& prefs) {
    nlohmann::json doc = {
        {"showGrid", prefs.showGrid},
        {"showStats", prefs.showStats},
        {"snapEnabled", prefs.snapEnabled},
        {"surfaceSnapEnabled", prefs.surfaceSnapEnabled},
        {"gridSize", prefs.gridSize},
        {"rotationSnapDegrees", prefs.rotationSnapDegrees},
        {"playMode", static_cast<int>(prefs.playMode)},
        {"pieNumberOfPlayers", ClampPlayers(prefs.pieNumberOfPlayers)},
        {"pieNetMode", static_cast<int>(prefs.pieNetMode)},
        {"pieClientAddress", prefs.pieClientAddress.empty() ? "127.0.0.1" : prefs.pieClientAddress},
        {"pieAspect", static_cast<int>(prefs.pieAspect)},
        {"viewportViewMode", static_cast<int>(prefs.viewportViewMode)},
        {"gizmoOp", static_cast<int>(prefs.gizmoOp)},
        {"gizmoSpace", static_cast<int>(prefs.gizmoSpace)},
        {"contentFavorites", prefs.contentFavoritePaths},
    };
    const std::string path = FilePath();
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Editor: cannot write " << path << '\n';
        return;
    }
    out << doc.dump(2) << '\n';
}

void LoadInto(EditorContext& ctx) {
    Apply(ctx, Load());
}

void SaveIfChanged(const EditorContext& ctx, EditorUserPreferences& ioLastSaved) {
    const EditorUserPreferences current = Capture(ctx);
    if (current.Equals(ioLastSaved)) {
        return;
    }
    Save(current);
    ioLastSaved = current;
}

} // namespace EditorPreferences
} // namespace leon::editor
