#include "Editor/Panels/FOutputLogPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <imgui.h>
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
        SelectedIndices.clear();
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

    void FOutputLogPanel::CopySelectedToClipboard() {
        if (SelectedIndices.empty()) {
            CopyAllVisibleToClipboard();
            return;
        }

        std::ostringstream ss;
        bool bFirst = true;
        for (size_t i = 0; i < Entries.size(); ++i) {
            if (SelectedIndices.find(i) == SelectedIndices.end())
                continue;
            if (!bFirst)
                ss << '\n';
            ss << FormatEntry(Entries[i]);
            bFirst = false;
        }
        ImGui::SetClipboardText(ss.str().c_str());
    }

    void FOutputLogPanel::CopyAllVisibleToClipboard() {
        std::string filter = FilterBuffer;
        std::transform(filter.begin(), filter.end(), filter.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::ostringstream ss;
        bool bFirst = true;
        for (const auto& entry : Entries) {
            if (entry.Level == ELogLevel::Info && !bShowInfo)
                continue;
            if (entry.Level == ELogLevel::Warning && !bShowWarnings)
                continue;
            if (entry.Level == ELogLevel::Error && !bShowErrors)
                continue;
            if (!filter.empty()) {
                std::string combined = FormatEntry(entry);
                std::transform(combined.begin(), combined.end(), combined.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (combined.find(filter) == std::string::npos)
                    continue;
            }
            if (!bFirst)
                ss << '\n';
            ss << FormatEntry(entry);
            bFirst = false;
        }
        ImGui::SetClipboardText(ss.str().c_str());
    }

    void FOutputLogPanel::Draw(bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow("  Output Log", bInOutOpen, ELucideIcon::FileText);

        try {
            if (ImGui::Button("Clear")) {
                Clear();
            }

            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                CopySelectedToClipboard();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Copy selected lines (or all visible). Ctrl+C also works.");
            }

            ImGui::SameLine();
            if (ImGui::Button("Copy All")) {
                CopyAllVisibleToClipboard();
            }

            ImGui::SameLine();
            ImGui::Checkbox("Info", &bShowInfo);
            ImGui::SameLine();
            ImGui::Checkbox("Warnings", &bShowWarnings);
            ImGui::SameLine();
            ImGui::Checkbox("Errors", &bShowErrors);

            ImGui::SameLine();
            ImGui::Checkbox("Auto-Scroll", &bAutoScroll);

            ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
            ImGui::SetNextItemWidth(190.0f);
            ImGui::InputTextWithHint("##LogFilter", "Filter log...", FilterBuffer, sizeof(FilterBuffer));

            ImGui::Separator();

            ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            const bool bLogFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            if (bLogFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) &&
                !ImGui::GetIO().WantTextInput) {
                CopySelectedToClipboard();
            }

            std::string filter = FilterBuffer;
            std::transform(filter.begin(), filter.end(), filter.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            for (size_t i = 0; i < Entries.size(); ++i) {
                const auto& entry = Entries[i];

                if (entry.Level == ELogLevel::Info && !bShowInfo)
                    continue;
                if (entry.Level == ELogLevel::Warning && !bShowWarnings)
                    continue;
                if (entry.Level == ELogLevel::Error && !bShowErrors)
                    continue;

                std::string lineText = FormatEntry(entry);
                if (!filter.empty()) {
                    std::string combined = lineText;
                    std::transform(combined.begin(), combined.end(), combined.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (combined.find(filter) == std::string::npos)
                        continue;
                }

                ImVec4 color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                ELucideIcon icon = ELucideIcon::FileText;
                ImU32 iconColor = IM_COL32(80, 160, 255, 255);

                if (entry.Level == ELogLevel::Warning) {
                    color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                    icon = ELucideIcon::Activity;
                    iconColor = IM_COL32(255, 200, 60, 255);
                } else if (entry.Level == ELogLevel::Error) {
                    color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                    icon = ELucideIcon::Zap;
                    iconColor = IM_COL32(255, 80, 80, 255);
                }

                ImVec2 curPos = ImGui::GetCursorScreenPos();
                FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x, curPos.y + 2.0f),
                                       ImVec2(curPos.x + 14.0f, curPos.y + 16.0f), icon, iconColor);
                ImGui::Dummy(ImVec2(16.0f, 16.0f));
                ImGui::SameLine();

                const bool bSelected = SelectedIndices.find(i) != SelectedIndices.end();
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(lineText.c_str(), bSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (bSelected)
                            SelectedIndices.erase(i);
                        else
                            SelectedIndices.insert(i);
                    } else {
                        SelectedIndices.clear();
                        SelectedIndices.insert(i);
                    }
                }
                if (ImGui::BeginPopupContextItem("LogLineContext")) {
                    if (ImGui::MenuItem("Copy")) {
                        if (!bSelected) {
                            SelectedIndices.clear();
                            SelectedIndices.insert(i);
                        }
                        CopySelectedToClipboard();
                    }
                    if (ImGui::MenuItem("Copy All Visible")) {
                        CopyAllVisibleToClipboard();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }

            if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
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
