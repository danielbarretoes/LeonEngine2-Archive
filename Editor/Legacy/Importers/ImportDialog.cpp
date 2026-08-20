#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <leon/core/Ascii.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/panels/ImportDialog.h>
#include <leon/editor/ui/UiKit.h>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

[[nodiscard]] const char* ModeLabel(EAssetImportMode mode) {
    switch (mode) {
    case EAssetImportMode::StaticObj:
        return "Static Mesh (OBJ)";
    case EAssetImportMode::StaticFbx:
        return "Static Mesh (FBX)";
    case EAssetImportMode::StaticGltf:
        return "Static Mesh (glTF)";
    case EAssetImportMode::CharacterFbx:
        return "Character (FBX skinned)";
    case EAssetImportMode::AnimFbx:
        return "Animation (FBX)";
    case EAssetImportMode::Texture2D:
        return "Texture2D";
    }
    return "Import";
}

[[nodiscard]] bool IsFbxMode(EAssetImportMode mode) {
    return mode == EAssetImportMode::StaticFbx || mode == EAssetImportMode::CharacterFbx ||
           mode == EAssetImportMode::AnimFbx;
}

[[nodiscard]] std::string ExtLowerOf(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return {};
    }
    return AsciiToLower(fs::path(path).extension().string());
}

} // namespace

void ImportDialog::Open() {
    // 1) File browser first — cancel leaves the dialog closed.
    const std::string picked = EditorPickOpenFile(
        "Importable\0*.fbx;*.obj;*.gltf;*.glb;*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.exr\0"
        "Autodesk FBX\0*.fbx\0"
        "Wavefront OBJ\0*.obj\0"
        "glTF\0*.gltf;*.glb\0"
        "Textures\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.exr\0"
        "All\0*.*\0",
        "Import Asset");
    if (picked.empty()) {
        return;
    }
    if (!IsImportableSourcePath(picked)) {
        EditorToast("Unsupported file type for import", EEditorToastKind::Error, 3.5f);
        return;
    }
    // 2) Detect format → options window for that extension.
    OpenWithSource(picked);
}

void ImportDialog::OpenWithSource(const std::string& sourcePath) {
    status_.clear();
    detectedHint_.clear();
    sourcePath_[0] = '\0';
    secondaryPath_[0] = '\0';
    jumpStartPath_[0] = '\0';
    fallLoopPath_[0] = '\0';
    landPath_[0] = '\0';
    skeletonPath_[0] = '\0';
    assetName_[0] = '\0';
    uniformScale_ = 1.0f;
    generateCollision_ = false;
    ApplySourcePath(sourcePath);
    if (sourcePath_[0] == '\0') {
        EditorToast("Could not read import source", EEditorToastKind::Error, 3.5f);
        open_ = false;
        return;
    }
    open_ = true;
    // OpenPopup is done in DrawOptionsModal (correct ImGui id stack).
}

void ImportDialog::ApplySourcePath(const std::string& sourcePath) {
    if (sourcePath.empty() || !IsImportableSourcePath(sourcePath)) {
        return;
    }
    const EAssetImportMode guessed = GuessImportMode(sourcePath);
    mode_ = static_cast<int>(guessed);
    std::string name;
    std::string secondary;
    std::string jumpStart;
    std::string fallLoop;
    std::string land;
    AutofillImportFromSource(sourcePath, guessed, name, secondary, jumpStart, fallLoop, land);

    std::string primary = sourcePath;
    if (guessed == EAssetImportMode::CharacterFbx && !name.empty()) {
        const fs::path meshCandidate = fs::path(sourcePath).parent_path() / (name + ".fbx");
        std::error_code ec;
        if (fs::is_regular_file(meshCandidate, ec) && !ec) {
            primary = meshCandidate.generic_string();
        }
    }
    (void)std::snprintf(sourcePath_, sizeof(sourcePath_), "%s", primary.c_str());
    (void)std::snprintf(assetName_, sizeof(assetName_), "%s", name.c_str());
    if (!secondary.empty()) {
        (void)std::snprintf(secondaryPath_, sizeof(secondaryPath_), "%s", secondary.c_str());
    } else {
        secondaryPath_[0] = '\0';
    }
    if (!jumpStart.empty()) {
        (void)std::snprintf(jumpStartPath_, sizeof(jumpStartPath_), "%s", jumpStart.c_str());
    } else {
        jumpStartPath_[0] = '\0';
    }
    if (!fallLoop.empty()) {
        (void)std::snprintf(fallLoopPath_, sizeof(fallLoopPath_), "%s", fallLoop.c_str());
    } else {
        fallLoopPath_[0] = '\0';
    }
    if (!land.empty()) {
        (void)std::snprintf(landPath_, sizeof(landPath_), "%s", land.c_str());
    } else {
        landPath_[0] = '\0';
    }
    detectedHint_ = std::string("Detected: ") + ModeLabel(guessed) + "  (" +
                    fs::path(primary).filename().generic_string() + ")";
    status_.clear();
}

const char* ImportDialog::OptionsWindowTitle() const {
    switch (static_cast<EAssetImportMode>(mode_)) {
    case EAssetImportMode::StaticObj:
    case EAssetImportMode::StaticFbx:
    case EAssetImportMode::StaticGltf:
        return "Import Static Mesh";
    case EAssetImportMode::CharacterFbx:
        return "Import Character";
    case EAssetImportMode::AnimFbx:
        return "Import Animation";
    case EAssetImportMode::Texture2D:
        return "Import Texture2D";
    }
    return "Import Asset";
}

void ImportDialog::DrawOptionsModal(EditorContext& ctx) {
    // Title bar = format-specific name; ### keeps a stable popup id across mode toggles.
    char popupTitle[96]{};
    (void)std::snprintf(popupTitle, sizeof(popupTitle), "%s###LeonImportOptions",
                        OptionsWindowTitle());
    if (open_) {
        ImGui::OpenPopup(popupTitle);
    }
    if (!ui::BeginModal(popupTitle, &open_)) {
        return;
    }

    ImGui::TextUnformatted("Destination (Content Browser folder)");
    if (!ctx.contentBrowserFolder.empty()) {
        ImGui::TextDisabled("%s", ctx.contentBrowserFolder.c_str());
    } else {
        ImGui::TextDisabled("(Content root)");
    }
    if (!detectedHint_.empty()) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "%s", detectedHint_.c_str());
    }
    ImGui::Separator();

    ImGui::Text("Source: %s", fs::path(sourcePath_).filename().generic_string().c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", sourcePath_);
    }
    ImGui::SameLine();
    if (ui::Button("##change_src", "Change…", ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
        const std::string picked = EditorPickOpenFile("Importable\0*.fbx;*.obj;*.gltf;*.glb\0"
                                                      "Autodesk FBX\0*.fbx\0"
                                                      "Wavefront OBJ\0*.obj\0"
                                                      "glTF\0*.gltf;*.glb\0"
                                                      "All\0*.*\0",
                                                      "Import Asset");
        if (!picked.empty() && IsImportableSourcePath(picked)) {
            ApplySourcePath(picked);
        }
    }

    const std::string ext = ExtLowerOf(sourcePath_);
    const int prevMode = mode_;

    // Options depend on extension: FBX can be Character / Static / Anim; others are fixed.
    if (ext == ".fbx") {
        ImGui::TextUnformatted("Import as");
        ImGui::RadioButton("Character (skinned)", &mode_,
                           static_cast<int>(EAssetImportMode::CharacterFbx));
        ImGui::SameLine();
        ImGui::RadioButton("Static Mesh", &mode_, static_cast<int>(EAssetImportMode::StaticFbx));
        ImGui::SameLine();
        ImGui::RadioButton("Animation", &mode_, static_cast<int>(EAssetImportMode::AnimFbx));
    } else if (ext == ".obj") {
        mode_ = static_cast<int>(EAssetImportMode::StaticObj);
        ImGui::TextDisabled("Options for Wavefront OBJ → Static Mesh");
    } else if (ext == ".gltf" || ext == ".glb") {
        mode_ = static_cast<int>(EAssetImportMode::StaticGltf);
        ImGui::TextDisabled("Options for glTF → Static Mesh");
    }

    if (mode_ != prevMode && IsFbxMode(static_cast<EAssetImportMode>(mode_)) &&
        sourcePath_[0] != '\0') {
        std::string name;
        std::string secondary;
        std::string jumpStart;
        std::string fallLoop;
        std::string land;
        AutofillImportFromSource(sourcePath_, static_cast<EAssetImportMode>(mode_), name, secondary,
                                 jumpStart, fallLoop, land);
        (void)std::snprintf(assetName_, sizeof(assetName_), "%s", name.c_str());
        if (mode_ == static_cast<int>(EAssetImportMode::CharacterFbx)) {
            (void)std::snprintf(secondaryPath_, sizeof(secondaryPath_), "%s", secondary.c_str());
            (void)std::snprintf(jumpStartPath_, sizeof(jumpStartPath_), "%s", jumpStart.c_str());
            (void)std::snprintf(fallLoopPath_, sizeof(fallLoopPath_), "%s", fallLoop.c_str());
            (void)std::snprintf(landPath_, sizeof(landPath_), "%s", land.c_str());
        } else {
            secondaryPath_[0] = '\0';
            jumpStartPath_[0] = '\0';
            fallLoopPath_[0] = '\0';
            landPath_[0] = '\0';
        }
        detectedHint_ = std::string("Mode: ") + ModeLabel(static_cast<EAssetImportMode>(mode_));
    }

    ImGui::Separator();
    (void)ui::InputText("##import_asset_name", "Asset Name", assetName_, sizeof(assetName_));

    if (mode_ == static_cast<int>(EAssetImportMode::CharacterFbx)) {
        (void)ui::InputText("##import_run_fbx", "Run / Locomotion FBX", secondaryPath_,
                            sizeof(secondaryPath_));
        ImGui::SameLine();
        if (ui::Button("##browse_run", "Browse", ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
            const std::string picked =
                EditorPickOpenFile("Autodesk FBX\0*.fbx\0All\0*.*\0", "Run FBX");
            if (!picked.empty()) {
                (void)std::snprintf(secondaryPath_, sizeof(secondaryPath_), "%s", picked.c_str());
            }
        }
        ImGui::TextDisabled("Optional. Auto-filled from *_Run.fbx / Running.fbx when present.");
        (void)ui::InputText("##import_jump", "Jump Start FBX", jumpStartPath_, sizeof(jumpStartPath_));
        (void)ui::InputText("##import_fall", "Fall Loop FBX", fallLoopPath_, sizeof(fallLoopPath_));
        (void)ui::InputText("##import_land", "Land FBX", landPath_, sizeof(landPath_));
        ImGui::TextDisabled(
            "Optional jump SM. Auto-filled from JumpingUp / FallingIdle / FallingToLanding.");
    }

    if (mode_ == static_cast<int>(EAssetImportMode::AnimFbx)) {
        (void)ui::InputText("##import_skel", "Skeleton (.lskel)", skeletonPath_, sizeof(skeletonPath_));
        ImGui::SameLine();
        if (ui::Button("##browse_skel", "Browse", ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
            const std::string picked =
                EditorPickOpenFile("Leon Skeleton\0*.lskel\0All\0*.*\0", "Skeleton");
            if (!picked.empty()) {
                (void)std::snprintf(skeletonPath_, sizeof(skeletonPath_), "%s", picked.c_str());
            }
        }
        (void)ui::Checkbox("##anim_loop", "Looping", &animLooping_, ui::EUiSize::Sm);
    }

    if (mode_ == static_cast<int>(EAssetImportMode::StaticObj) ||
        mode_ == static_cast<int>(EAssetImportMode::StaticFbx) ||
        mode_ == static_cast<int>(EAssetImportMode::StaticGltf)) {
        ImGui::InputFloat("Uniform Scale", &uniformScale_, 0.01f, 0.1f, "%.3f");
        if (uniformScale_ <= 0.0f) {
            uniformScale_ = 1.0f;
        }
    }

    if (!status_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_.c_str());
    }

    ui::Separator();
    if (ui::DialogButton("##import", "Import", ui::EUiVariant::Primary)) {
        AssetImportRequest req;
        req.mode = static_cast<EAssetImportMode>(mode_);
        req.sourcePath = sourcePath_;
        req.secondaryFbxPath = secondaryPath_;
        req.jumpStartFbxPath = jumpStartPath_;
        req.fallLoopFbxPath = fallLoopPath_;
        req.landFbxPath = landPath_;
        req.skeletonJsonPath = skeletonPath_;
        req.assetName = assetName_;
        req.animLooping = animLooping_;
        req.uniformScale = uniformScale_;
        req.generateCollision = generateCollision_;

        AssetImportResult result;
        if (EditorImportAsset(ctx, req, result)) {
            status_ = result.message;
            ctx.previewAssetPath = result.previewPath;
            ctx.requestContentRefresh = true;
            ctx.requestPreviewReload = true;
            EditorToast(result.message, EEditorToastKind::Success, 3.0f);
            open_ = false;
            ImGui::CloseCurrentPopup();
        } else {
            status_ = result.message.empty() ? "Import failed" : result.message;
            EditorToast(status_, EEditorToastKind::Error, 4.5f);
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ui::EndModal();
}

void ImportDialog::Draw(EditorContext& ctx) {
    if (!ctx.pendingImportSourcePath.empty()) {
        OpenWithSource(ctx.pendingImportSourcePath);
        ctx.pendingImportSourcePath.clear();
    }

    if (open_) {
        DrawOptionsModal(ctx);
    }
}

} // namespace leon::editor
