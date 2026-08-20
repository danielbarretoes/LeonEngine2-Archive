#include "Editor/Panels/FContentBrowserPanel.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace Leon::Editor {

    void FContentBrowserPanel::SetContentDirectory(const std::string& InContentDir) {
        if (!InContentDir.empty() && fs::exists(InContentDir)) {
            BaseContentPath = fs::canonical(InContentDir);
            CurrentDirectory = BaseContentPath;
        } else {
            BaseContentPath.clear();
            CurrentDirectory.clear();
        }
    }

    void FContentBrowserPanel::Draw() {
        ImGui::Begin("Content Browser");

        if (BaseContentPath.empty() || !fs::exists(BaseContentPath)) {
            ImGui::TextDisabled("No active project loaded or Content folder missing.");
            ImGui::End();
            return;
        }

        // Top Navigation Bar
        bool bCanGoBack = (CurrentDirectory != BaseContentPath && CurrentDirectory.has_parent_path());
        if (!bCanGoBack)
            ImGui::BeginDisabled();
        if (ImGui::Button("< Back") && bCanGoBack) {
            CurrentDirectory = CurrentDirectory.parent_path();
        }
        if (!bCanGoBack)
            ImGui::EndDisabled();

        ImGui::SameLine();

        // Relative path breadcrumbs
        std::string relPath = "/Game";
        try {
            auto sub = fs::relative(CurrentDirectory, BaseContentPath).string();
            if (sub != "." && !sub.empty()) {
                for (char c : sub) {
                    if (c == '\\')
                        relPath += "/";
                    else
                        relPath += c;
                }
            }
        } catch (...) {
        }

        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", relPath.c_str());

        ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##ContentSearch", "Search content...", SearchBuffer, sizeof(SearchBuffer));

        ImGui::Separator();
        ImGui::Spacing();

        // 2 Columns: Folder tree on left, items on right
        ImGui::Columns(2, "ContentBrowserColumns", true);
        ImGui::SetColumnWidth(0, 180.0f);

        ImGui::TextUnformatted("Content Folders");
        ImGui::Separator();
        DrawDirectoryTree(BaseContentPath);

        ImGui::NextColumn();

        DrawAssetGrid();

        ImGui::Columns(1);
        ImGui::End();
    }

    void FContentBrowserPanel::DrawDirectoryTree(const fs::path& InDir) {
        if (!fs::exists(InDir))
            return;

        bool bIsCurrent = (InDir == CurrentDirectory);
        std::string folderName = (InDir == BaseContentPath) ? "Content" : InDir.filename().string();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsCurrent)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool bHasSubdirs = false;
        for (const auto& entry : fs::directory_iterator(InDir)) {
            if (entry.is_directory()) {
                bHasSubdirs = true;
                break;
            }
        }

        if (!bHasSubdirs)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool bOpen = ImGui::TreeNodeEx(folderName.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            CurrentDirectory = InDir;
        }

        if (bOpen && bHasSubdirs) {
            for (const auto& entry : fs::directory_iterator(InDir)) {
                if (entry.is_directory()) {
                    DrawDirectoryTree(entry.path());
                }
            }
            ImGui::TreePop();
        }
    }

    void FContentBrowserPanel::DrawAssetGrid() {
        if (!fs::exists(CurrentDirectory))
            return;

        std::string search = SearchBuffer;
        std::transform(search.begin(), search.end(), search.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        float panelWidth = ImGui::GetContentRegionAvail().x;
        float itemSize = 100.0f;
        int columns = std::max(1, static_cast<int>(panelWidth / (itemSize + 16.0f)));

        ImGui::BeginChild("AssetGridChild", ImVec2(0, 0), false);

        int colIdx = 0;
        for (const auto& entry : fs::directory_iterator(CurrentDirectory)) {
            std::string filename = entry.path().filename().string();
            if (filename.empty() || filename[0] == '.')
                continue;

            if (!search.empty()) {
                std::string lower = filename;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lower.find(search) == std::string::npos)
                    continue;
            }

            ImGui::PushID(filename.c_str());

            bool bIsDir = entry.is_directory();
            std::string ext = entry.path().extension().string();

            ImVec4 badgeColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            const char* typeBadge = "[File]";

            if (bIsDir) {
                badgeColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
                typeBadge = "[Folder]";
            } else if (ext == ".lmap") {
                badgeColor = ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
                typeBadge = "[Map]";
            } else if (ext == ".lmat") {
                badgeColor = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
                typeBadge = "[Material]";
            } else if (ext == ".lmesh" || ext == ".obj" || ext == ".gltf") {
                badgeColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
                typeBadge = "[Mesh]";
            } else if (ext == ".ltex" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                badgeColor = ImVec4(0.8f, 0.3f, 0.8f, 1.0f);
                typeBadge = "[Texture]";
            } else if (ext == ".lhdr" || ext == ".hdr") {
                badgeColor = ImVec4(0.2f, 0.9f, 0.9f, 1.0f);
                typeBadge = "[Sky HDR]";
            }

            ImGui::BeginGroup();

            // Box item button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
            if (ImGui::Button(typeBadge, ImVec2(itemSize, 60.0f))) {
                if (bIsDir) {
                    CurrentDirectory = entry.path();
                } else if (ext == ".lmap") {
                    if (OnMapSelected)
                        OnMapSelected(entry.path().string());
                }
            }
            ImGui::PopStyleColor();

            // Double click to open
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (bIsDir) {
                    CurrentDirectory = entry.path();
                } else if (ext == ".lmap") {
                    if (OnMapSelected)
                        OnMapSelected(entry.path().string());
                }
            }

            // Filename label
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + itemSize);
            ImGui::TextColored(badgeColor, "%s", filename.c_str());
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();

            colIdx++;
            if (colIdx % columns != 0) {
                ImGui::SameLine();
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

} // namespace Leon::Editor
