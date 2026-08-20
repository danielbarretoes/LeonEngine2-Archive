#include "Editor/Panels/FOutputLogPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <algorithm>
#include <cctype>
#include <imgui.h>

namespace Leon::Editor {

    void FOutputLogPanel::AddLog(ELogLevel InLevel, const std::string& InCategory, const std::string& InMessage) {
        FLogEntry entry;
        entry.Level = InLevel;
        entry.Category = InCategory;
        entry.Message = InMessage;
        Entries.push_back(entry);
    }

    void FOutputLogPanel::Clear() {
        Entries.clear();
    }

    void FOutputLogPanel::Draw() {
        ImGui::Begin("Output Log");

        // Top Controls: Clear, Filter checkboxes, Search box
        if (ImGui::Button("Clear")) {
            Clear();
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

        // Log Messages list
        ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

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

            if (!filter.empty()) {
                std::string lowerMsg = entry.Message;
                std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lowerMsg.find(filter) == std::string::npos)
                    continue;
            }

            ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            ELucideIcon icon = ELucideIcon::Activity;
            ImU32 iconColor = IM_COL32(100, 180, 255, 255);

            if (entry.Level == ELogLevel::Warning) {
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                icon = ELucideIcon::Zap;
                iconColor = IM_COL32(255, 200, 50, 255);
            } else if (entry.Level == ELogLevel::Error) {
                color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                icon = ELucideIcon::Skull;
                iconColor = IM_COL32(255, 80, 80, 255);
            }

            ImVec2 curPos = ImGui::GetCursorScreenPos();
            FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x, curPos.y + 2.0f),
                                   ImVec2(curPos.x + 14.0f, curPos.y + 16.0f), icon, iconColor);
            ImGui::Dummy(ImVec2(16.0f, 16.0f));
            ImGui::SameLine();

            ImGui::TextColored(color, "[%s] %s", entry.Category.empty() ? "Core" : entry.Category.c_str(),
                               entry.Message.c_str());
        }

        if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace Leon::Editor
