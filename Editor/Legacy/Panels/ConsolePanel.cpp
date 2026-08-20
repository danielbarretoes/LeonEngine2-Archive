#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/ConsolePanel.h>
#include <leon/editor/ui/UiKit.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace {

constexpr const char* kCommands[] = {"help", "clear", "focus", "FocusSelected", "stat", "openmat "};

void CopyLogSnapshotToClipboard() {
    const auto all = EditorOutputLog::Instance().Snapshot();
    std::string text;
    text.reserve(all.size() * 64);
    for (const EditorLogLine& line : all) {
        text += line.text;
        text.push_back('\n');
    }
    ImGui::SetClipboardText(text.c_str());
}

} // namespace

int ConsolePanel::TextCallback(ImGuiInputTextCallbackData* data) {
    auto* self = static_cast<ConsolePanel*>(data->UserData);
    if (self == nullptr) {
        return 0;
    }
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (self->history_.empty()) {
            return 0;
        }
        if (self->historyBrowse_ < 0) {
            (void)std::snprintf(self->draft_, sizeof(self->draft_), "%.*s", data->BufTextLen,
                                data->Buf);
        }
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (self->historyBrowse_ < 0) {
                self->historyBrowse_ = static_cast<int>(self->history_.size()) - 1;
            } else if (self->historyBrowse_ > 0) {
                --self->historyBrowse_;
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (self->historyBrowse_ < 0) {
                return 0;
            }
            if (self->historyBrowse_ + 1 >= static_cast<int>(self->history_.size())) {
                self->historyBrowse_ = -1;
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, self->draft_);
                return 0;
            }
            ++self->historyBrowse_;
        }
        if (self->historyBrowse_ >= 0 &&
            self->historyBrowse_ < static_cast<int>(self->history_.size())) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(
                0, self->history_[static_cast<std::size_t>(self->historyBrowse_)].c_str());
        }
        return 0;
    }
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        const std::string prefix(data->Buf, static_cast<std::size_t>(data->CursorPos));
        const char* match = nullptr;
        int matchCount = 0;
        for (const char* cmd : kCommands) {
            if (std::strncmp(cmd, prefix.c_str(), prefix.size()) == 0) {
                match = cmd;
                ++matchCount;
            }
        }
        if (matchCount == 1 && match != nullptr) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, match);
        } else if (matchCount > 1) {
            EditorLogInfo("Candidates:");
            for (const char* cmd : kCommands) {
                if (std::strncmp(cmd, prefix.c_str(), prefix.size()) == 0) {
                    EditorLogInfo(std::string("  ") + cmd);
                }
            }
        }
    }
    return 0;
}

void ConsolePanel::Execute(EditorContext& ctx, const std::string& line) {
    if (line.empty()) {
        return;
    }
    if (history_.empty() || history_.back() != line) {
        history_.push_back(line);
        constexpr std::size_t kMaxHistory = 64;
        if (history_.size() > kMaxHistory) {
            history_.erase(history_.begin());
        }
    }
    historyBrowse_ = -1;
    draft_[0] = '\0';

    EditorLogInfo("> " + line);

    if (line == "help" || line == "?") {
        EditorLogInfo("Commands: help, clear, focus, openmat <path.lmat>, stat");
        return;
    }
    if (line == "clear") {
        EditorOutputLog::Instance().Clear();
        return;
    }
    if (line == "focus" || line == "FocusSelected") {
        ctx.RequestFocusSelected();
        EditorLogInfo("Focus Selected requested");
        return;
    }
    if (line == "stat") {
        EditorLogInfo(std::string("dirty=") + (ctx.dirty ? "1" : "0") +
                      " level=" + (ctx.levelPath.empty() ? "(unsaved)" : ctx.levelPath));
        return;
    }
    constexpr const char* kOpenMat = "openmat ";
    if (line.rfind(kOpenMat, 0) == 0) {
        const std::string path = line.substr(std::strlen(kOpenMat));
        if (path.empty()) {
            EditorLogWarn("Usage: openmat <path.lmat>");
            return;
        }
        ctx.requestOpenMaterialPath = path;
        EditorLogInfo("Opening material " + path);
        return;
    }

    EditorLogWarn("Unknown command. Type 'help'.");
}

void ConsolePanel::Draw(EditorContext& ctx) {
    if (!ctx.showConsole) {
        return;
    }
    if (ctx.requestFocusConsole) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusConsole = false;
    }
    if (!ui::BeginPanel("Console", {.pOpen = &ctx.showConsole})) {
        ui::EndPanel();
        return;
    }

    ui::Hint("Commands: help · clear · focus · stat · openmat <path.lmat>");

    if (ui::Button("##clear", "Clear", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
        EditorOutputLog::Instance().Clear();
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc copy;
        copy.label = "Copy";
        copy.icon = ui::UiIcon(ELucideIcon::FileText);
        copy.variant = ui::EUiVariant::Secondary;
        copy.size = ui::EUiSize::Sm;
        if (ui::Button("##copy", copy)) {
            CopyLogSnapshotToClipboard();
        }
    }
    ImGui::SameLine();
    (void)ui::Checkbox("##autoscroll", "Auto-scroll", &autoScroll_, ui::EUiSize::Sm);
    ImGui::SameLine();
    ImGui::TextDisabled("Tip: Tab complete · Up/Down history · help");

    ImGui::Separator();
    const float footer = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    if (ImGui::BeginChild("##console_scroll", ImVec2(0, -footer), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_C) && !ImGui::GetIO().WantTextInput) {
            CopyLogSnapshotToClipboard();
        }
        const auto all = EditorOutputLog::Instance().Snapshot();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(all.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const EditorLogLine& line = all[static_cast<std::size_t>(i)];
                ImVec4 color{0.85f, 0.85f, 0.85f, 1.0f};
                if (line.level == EEditorLogLevel::Warning) {
                    color = ImVec4{0.95f, 0.8f, 0.25f, 1.0f};
                } else if (line.level == EEditorLogLevel::Error) {
                    color = ImVec4{0.95f, 0.35f, 0.3f, 1.0f};
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushID(i);
                ImGui::Selectable(line.text.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick |
                                      ImGuiSelectableFlags_SpanAllColumns);
                if (ImGui::BeginPopupContextItem("##console_line_ctx")) {
                    if (ImGui::MenuItem("Copy Line")) {
                        ImGui::SetClipboardText(line.text.c_str());
                    }
                    if (ImGui::MenuItem("Copy All")) {
                        CopyLogSnapshotToClipboard();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }
        }
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-1.0f);
    ui::UiInputDesc consoleInput;
    consoleInput.width = -1.0f;
    consoleInput.flags = ImGuiInputTextFlags_EnterReturnsTrue |
                         ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory |
                         ImGuiInputTextFlags_CallbackCompletion;
    consoleInput.callback = &ConsolePanel::TextCallback;
    consoleInput.callbackUserData = this;
    const bool submit =
        ui::InputText("##console_input", nullptr, input_, sizeof(input_), consoleInput);
    if (submit) {
        Execute(ctx, input_);
        input_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    ui::EndPanel();
}

} // namespace leon::editor
