#pragma once

#include <leon/editor/EditorContext.h>
#include <string>

namespace leon::editor {

/// Import flow (Unreal-like): file picker first → options window for the detected format.
class ImportDialog {
public:
    /// Opens the OS file browser; on pick, shows the options modal for that extension.
    void Open();
    /// Skip the picker (e.g. Content Browser double-click / RMB Import).
    void OpenWithSource(const std::string& sourcePath);
    void Draw(EditorContext& ctx);

    [[nodiscard]] bool IsOpen() const { return open_; }

private:
    void ApplySourcePath(const std::string& sourcePath);
    void DrawOptionsModal(EditorContext& ctx);
    [[nodiscard]] const char* OptionsWindowTitle() const;

    bool open_ = false;
    int mode_ = 0; // EAssetImportMode
    char sourcePath_[512]{};
    char secondaryPath_[512]{};
    char jumpStartPath_[512]{};
    char fallLoopPath_[512]{};
    char landPath_[512]{};
    char skeletonPath_[512]{};
    char assetName_[128]{};
    bool animLooping_ = true;
    float uniformScale_ = 1.0f;
    bool generateCollision_ = false;
    std::string status_;
    std::string detectedHint_;
};

} // namespace leon::editor
