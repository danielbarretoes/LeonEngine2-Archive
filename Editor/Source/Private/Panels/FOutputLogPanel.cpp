#include "Editor/Panels/FOutputLogPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <imgui.h>
#include <imgui_internal.h>
#include <sstream>

namespace Leon::Editor {

    namespace {

        std::string NowTimestamp() {
            using clock = std::chrono::system_clock;
            const auto now = clock::now();
            const std::time_t t = clock::to_time_t(now);
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &t);
#else
            localtime_r(&t, &local);
#endif
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
            return buf;
        }

        std::string ToLowerCopy(std::string InText) {
            std::transform(InText.begin(), InText.end(), InText.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return InText;
        }

    } // namespace

    void FOutputLogPanel::AddLog(ELogLevel InLevel, const std::string& InCategory, const std::string& InMessage) {
        FLogEntry entry;
        entry.Level = InLevel;
        entry.Category = InCategory;
        entry.Message = InMessage;
        entry.Timestamp = NowTimestamp();
        Entries.push_back(entry);
    }

    void FOutputLogPanel::Clear() {
        Entries.clear();
        VisibleLogText.clear();
        CachedSelection.clear();
        bWasAtBottom = true;
    }

    std::string FOutputLogPanel::FormatEntry(const FLogEntry& InEntry) const {
        const char* level = "Info";
        if (InEntry.Level == ELogLevel::Warning)
            level = "Warning";
        else if (InEntry.Level == ELogLevel::Error)
            level = "Error";
        std::ostringstream ss;
        if (!InEntry.Timestamp.empty())
            ss << InEntry.Timestamp << " ";
        ss << "[" << level << "] [" << InEntry.Category << "] " << InEntry.Message;
        return ss.str();
    }

    std::string FOutputLogPanel::MakeFilterLower() const {
        return ToLowerCopy(FilterBuffer);
    }

    bool FOutputLogPanel::PassesFilters(const FLogEntry& InEntry, const std::string& InFilterLower) const {
        if (InEntry.Level == ELogLevel::Info && !bShowInfo)
            return false;
        if (InEntry.Level == ELogLevel::Warning && !bShowWarnings)
            return false;
        if (InEntry.Level == ELogLevel::Error && !bShowErrors)
            return false;
        if (InFilterLower.empty())
            return true;
        return ToLowerCopy(FormatEntry(InEntry)).find(InFilterLower) != std::string::npos;
    }

    void FOutputLogPanel::RebuildVisibleText() {
        const std::string filter = MakeFilterLower();
        VisibleLogText.clear();
        bool bFirst = true;
        for (const auto& entry : Entries) {
            if (!PassesFilters(entry, filter))
                continue;
            if (!bFirst)
                VisibleLogText += '\n';
            VisibleLogText += FormatEntry(entry);
            bFirst = false;
        }
    }

    void FOutputLogPanel::CacheSelectionFromActiveLog() {
        CachedSelection.clear();
        ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID());
        if (!state || !state->HasSelection())
            return;

        int start = state->GetSelectionStart();
        int end = state->GetSelectionEnd();
        if (start > end)
            std::swap(start, end);
        start = std::max(start, 0);
        end = std::min(end, state->TextLen);
        if (end <= start)
            return;

        const char* src = state->TextA.Data ? state->TextA.Data : VisibleLogText.c_str();
        CachedSelection.assign(src + start, src + end);
    }

    void FOutputLogPanel::CopySelectedToClipboard() {
        if (!CachedSelection.empty()) {
            ImGui::SetClipboardText(CachedSelection.c_str());
            return;
        }
        CopyAllVisibleToClipboard();
    }

    void FOutputLogPanel::CopyAllVisibleToClipboard() {
        RebuildVisibleText();
        ImGui::SetClipboardText(VisibleLogText.c_str());
    }

    void FOutputLogPanel::Draw(bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::OutputLog, bInOutOpen, ELucideIcon::FileText);

        try {
            if (FEditorWidgets::DrawButton(ELucideIcon::Trash, "##LogClear", "Clear")) {
                Clear();
            }

            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::Copy, "##LogCopy", "Copy")) {
                CopySelectedToClipboard();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Copy selected text. If nothing is selected, copy all visible lines.");
            }

            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::Copy, "##LogCopyAll", "Copy All")) {
                CopyAllVisibleToClipboard();
            }

            ImGui::SameLine();
            FEditorWidgets::DrawCheckbox("##ShowInfo", &bShowInfo, "Info");
            ImGui::SameLine();
            FEditorWidgets::DrawCheckbox("##ShowWarnings", &bShowWarnings, "Warnings");
            ImGui::SameLine();
            FEditorWidgets::DrawCheckbox("##ShowErrors", &bShowErrors, "Errors");

            ImGui::SameLine();
            FEditorWidgets::DrawCheckbox("##AutoScroll", &bAutoScroll, "Auto-Scroll");

            ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
            {
                FControlStyle style;
                style.Width = 190.0f;
                FEditorWidgets::DrawSearchInput("LogFilter", FilterBuffer, sizeof(FilterBuffer), "Filter log...",
                                                &style);
            }

            ImGui::Separator();

            RebuildVisibleText();

            const bool bStickToBottom = bAutoScroll && bWasAtBottom && CachedSelection.empty();
            if (bStickToBottom)
                ImGui::SetNextWindowScroll(ImVec2(-1.0f, FLT_MAX));

            const ImVec4 logBg = FEditorTheme::GetTokens().Card;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, logBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, logBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, logBg);
            ImGui::PushStyleColor(ImGuiCol_Text, FEditorTheme::GetTokens().Foreground);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            if (FEditorTheme::FontSmall)
                ImGui::PushFont(FEditorTheme::FontSmall);

            ImGui::InputTextMultiline("##OutputLogText", VisibleLogText.data(), VisibleLogText.size() + 1,
                                      ImVec2(-FLT_MIN, -FLT_MIN),
                                      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);

            CacheSelectionFromActiveLog();

            ImGuiWindow* parent = ImGui::GetCurrentWindow();
            char childName[256];
            ImFormatString(childName, IM_ARRAYSIZE(childName), "%s/%s", parent->Name, "##OutputLogText");
            if (ImGuiWindow* child = ImGui::FindWindowByName(childName)) {
                bWasAtBottom =
                    child->ScrollMax.y <= 1.0f || child->Scroll.y >= child->ScrollMax.y - 4.0f;
            }

            if (FEditorTheme::FontSmall)
                ImGui::PopFont();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FOutputLogPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Output Log Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FOutputLogPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Output Log: Unknown error encountered");
        }

        ImGui::End();
    }

} // namespace Leon::Editor
