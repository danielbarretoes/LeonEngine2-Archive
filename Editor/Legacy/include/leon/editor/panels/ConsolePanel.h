#pragma once

#include <leon/editor/EditorContext.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Unreal-like Console: shared Output Log feed + command line.
class ConsolePanel {
public:
    void Draw(EditorContext& ctx);

private:
    bool autoScroll_ = true;
    char input_[256]{};
    std::vector<std::string> history_;
    int historyBrowse_ = -1; // -1 = live buffer; else index into history_
    char draft_[256]{};

    void Execute(EditorContext& ctx, const std::string& line);
    static int TextCallback(ImGuiInputTextCallbackData* data);
};

} // namespace leon::editor
