#include "Editor/Panels/FContentBrowserPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <algorithm>
#include <cctype>
#include <imgui.h>

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
        float itemSize = 90.0f;
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

            ImVec4 badgeColor = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
            ELucideIcon icon = ELucideIcon::FileText;
            ImU32 iconColor = IM_COL32(180, 180, 190, 255);

            if (bIsDir) {
                badgeColor = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
                icon = ELucideIcon::Folder;
                iconColor = IM_COL32(240, 190, 60, 255);
            } else if (ext == ".lmap") {
                badgeColor = ImVec4(1.0f, 0.45f, 0.25f, 1.0f);
                icon = ELucideIcon::Map;
                iconColor = IM_COL32(255, 110, 60, 255);
            } else if (ext == ".lmat") {
                badgeColor = ImVec4(0.3f, 0.85f, 0.45f, 1.0f);
                icon = ELucideIcon::Layers;
                iconColor = IM_COL32(80, 220, 120, 255);
            } else if (ext == ".lmesh" || ext == ".obj" || ext == ".gltf") {
                badgeColor = ImVec4(0.35f, 0.65f, 1.0f, 1.0f);
                icon = ELucideIcon::Box;
                iconColor = IM_COL32(90, 170, 255, 255);
            } else if (ext == ".ltex" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                badgeColor = ImVec4(0.85f, 0.4f, 0.85f, 1.0f);
                icon = ELucideIcon::Image;
                iconColor = IM_COL32(220, 100, 220, 255);
            } else if (ext == ".lhdr" || ext == ".hdr") {
                badgeColor = ImVec4(0.3f, 0.95f, 0.95f, 1.0f);
                icon = ELucideIcon::Sun;
                iconColor = IM_COL32(80, 240, 240, 255);
            }

            ImGui::BeginGroup();

            // Box item button with Lucide icon inside
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.21f, 1.0f));
            bool bClicked = ImGui::Button("##asset_btn", ImVec2(itemSize, 56.0f));
            ImGui::PopStyleColor();

            ImVec2 btnMin = ImGui::GetItemRectMin();
            ImVec2 btnMax = ImGui::GetItemRectMax();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            FLucideIcons::DrawIcon(draw, ImVec2(btnMin.x + (itemSize - 30.0f) * 0.5f, btnMin.y + 12.0f),
                                   ImVec2(btnMin.x + (itemSize + 30.0f) * 0.5f, btnMin.y + 42.0f), icon, iconColor);

            if (bClicked) {
                if (bIsDir) {
                    CurrentDirectory = entry.path();
                } else if (ext == ".lmap") {
                    if (OnMapSelected)
                        OnMapSelected(entry.path().string());
                }
            }

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
