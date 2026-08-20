#pragma once

#include <string>

namespace leon::editor {

struct EditorContext;

/// Result of a Content Browser / File → Import operation.
struct AssetImportResult {
    bool ok = false;
    std::string message;
    /// Primary asset path to preview after import (.lmesh, .lchar, .lanim, …).
    std::string previewPath;
    /// Cooked asset path (often same as previewPath for static meshes).
    std::string cookedPath;
    /// Folder containing the imported asset pack.
    std::string importFolder;
};

enum class EAssetImportMode : int {
    StaticObj = 0,
    CharacterFbx = 1,
    AnimFbx = 2,
    StaticFbx = 3,
    StaticGltf = 4,
    Texture2D = 5,
};

struct AssetImportRequest {
    EAssetImportMode mode = EAssetImportMode::StaticObj;
    std::string sourcePath;
    /// Character: optional second FBX for run/locomotion (defaults to sourcePath).
    std::string secondaryFbxPath;
    /// Character: optional Mixamo jump / fall / land FBXs (cooked into AnimInstance clips).
    std::string jumpStartFbxPath;
    std::string fallLoopFbxPath;
    std::string landFbxPath;
    /// Anim: existing `*.lskel`.
    std::string skeletonJsonPath;
    std::string assetName;
    bool animLooping = true;
    float uniformScale = 1.0f;
    bool generateCollision = false;
    /// When set (Reimport), cook into this folder instead of the Content Browser folder.
    std::string destinationFolderOverride;
};

/// Unreal-like import sidecar written next to cooked assets (`<Asset>.leonimport`).
struct AssetImportSidecar {
    EAssetImportMode mode = EAssetImportMode::StaticObj;
    std::string sourcePath;
    std::string secondaryFbxPath;
    std::string jumpStartFbxPath;
    std::string fallLoopFbxPath;
    std::string landFbxPath;
    std::string skeletonJsonPath;
    std::string assetName;
    bool animLooping = true;
    float uniformScale = 1.0f;
    bool generateCollision = false;
};

/// True for DCC / image sources that File → Import can cook
/// (.obj / .fbx / .gltf / .glb / .png / .jpg / .exr / …).
[[nodiscard]] bool IsImportableSourcePath(const std::string& path);

/// Pick import mode from file extension + sibling heuristics (Idle/Run/Walk FBX → Character).
[[nodiscard]] EAssetImportMode GuessImportMode(const std::string& sourcePath);

/// Fill asset name + Character Run / jump FBXs from siblings next to `sourcePath`.
void AutofillImportFromSource(const std::string& sourcePath, EAssetImportMode mode,
                              std::string& outAssetName, std::string& outSecondaryFbx,
                              std::string& outJumpStartFbx, std::string& outFallLoopFbx,
                              std::string& outLandFbx);

/// Sidecar path beside a cooked asset (`.lchar` / `.lmesh` / `.lanim` / `.lskm`).
[[nodiscard]] std::string ImportSidecarPathForAsset(const std::string& cookedAssetPath);

[[nodiscard]] bool LoadImportSidecar(const std::string& sidecarPath, AssetImportSidecar& out);
[[nodiscard]] bool SaveImportSidecar(const EditorContext& ctx, const std::string& cookedAssetPath,
                                     const AssetImportRequest& request);

/// True when a `.leonimport` sidecar exists for this cooked asset.
[[nodiscard]] bool HasImportSidecar(const std::string& cookedAssetPath);

/// Reimport from the sidecar next to `cookedAssetPath` (Unreal Reimport).
[[nodiscard]] bool EditorReimportAsset(EditorContext& ctx, const std::string& cookedAssetPath,
                                       AssetImportResult& out);

/// Import into the Content Browser's open folder (`ctx.contentBrowserFolder`).
/// `Raw/` folders resolve to their parent (character pack root). Falls back to pack `Content/`.
[[nodiscard]] bool EditorImportAsset(EditorContext& ctx, const AssetImportRequest& request,
                                     AssetImportResult& out);

} // namespace leon::editor
