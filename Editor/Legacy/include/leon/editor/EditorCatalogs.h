#pragma once

#include <string>
#include <vector>

namespace leon::editor {

struct EditorMaterialEntry {
    std::string displayName;
    /// Path used for loadMaterial / level authoring (often relative like Materials/M_Default.lmat).
    std::string authoringPath;
    /// Absolute path when available (for preview / Material Editor).
    std::string absolutePath;
};

struct EditorSkyboxEntry {
    std::string displayName;
    /// Path stored on the level / passed to loadEnvMap.
    std::string authoringPath;
    std::string absolutePath;
};

struct EditorBlueprintEntry {
    std::string displayName; // BP_<Name>
    /// Pack-relative path for PlaceBlueprintInLevel (e.g. Blueprints/BP_Foo.lbp).
    std::string authoringPath;
    std::string absolutePath;
};

struct EditorTextureEntry {
    std::string displayName;
    /// Pack- or engine-relative path stored on materials (e.g. Textures/T_Grass/T_Grass.ltx).
    std::string authoringPath;
    std::string absolutePath;
};

/// Authoring GameMode ids for World Settings / Project Settings combos:
/// built-ins + Config/*.ini (`GlobalDefaultGameMode` + `GameModes[]`) + ids found on levels.
/// `gameModes` is a string-id catalog (not a levels list) — packs list C++ registration ids
/// because there is no UClass-style reflection scan of the gameplay DLL.
[[nodiscard]] std::vector<std::string> ListEditorGameModes(const std::string& projectPath = {});

/// Collect `.lmat` assets from Engine + open project pack.
[[nodiscard]] std::vector<EditorMaterialEntry>
CollectEditorMaterials(const std::string& projectPath);

/// Collect HDR skyboxes from Engine + open project pack.
[[nodiscard]] std::vector<EditorSkyboxEntry> CollectEditorSkyboxes(const std::string& projectPath);

/// Collect `BP_*.lbp` under Content/Blueprints (and nested folders).
[[nodiscard]] std::vector<EditorBlueprintEntry>
CollectEditorBlueprints(const std::string& projectPath);

/// Collect cooked `.ltx` + common source images from Engine Textures + pack Content.
[[nodiscard]] std::vector<EditorTextureEntry> CollectEditorTextures(const std::string& projectPath);

} // namespace leon::editor
