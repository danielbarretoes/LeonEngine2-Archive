#include "Editor/Panels/FContentBrowserPanel.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Editor/Utils/FEditorFileDialog.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <glad/glad.h>
#include <stb_image.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <imgui.h>
#include <sstream>

namespace fs = std::filesystem;

namespace Leon::Editor {

    FContentBrowserPanel::~FContentBrowserPanel() {
        DestroyThumbnails();
    }

    void FContentBrowserPanel::DestroyThumbnails() {
        for (auto& [path, texId] : TextureThumbnailCache) {
            if (texId != 0) {
                glDeleteTextures(1, &texId);
            }
        }
        TextureThumbnailCache.clear();
    }

    uint32_t FContentBrowserPanel::GetOrCreateTextureThumbnail(const std::string& InPath) {
        auto it = TextureThumbnailCache.find(InPath);
        if (it != TextureThumbnailCache.end()) {
            return it->second;
        }

        std::error_code ec;
        if (!fs::exists(InPath, ec) || ec) {
            TextureThumbnailCache[InPath] = 0;
            return 0;
        }

        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* pixels = stbi_load(InPath.c_str(), &w, &h, &channels, 4);
        if (!pixels || w <= 0 || h <= 0) {
            if (pixels)
                stbi_image_free(pixels);
            TextureThumbnailCache[InPath] = 0;
            return 0;
        }

        uint32_t texId = 0;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);

        TextureThumbnailCache[InPath] = texId;
        return texId;
    }

    void FContentBrowserPanel::DrawTextureThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax,
                                                    const fs::path& InPath) {
        uint32_t texId = GetOrCreateTextureThumbnail(InPath.string());
        if (texId != 0) {
            InDrawList->AddImageRounded(static_cast<ImTextureID>(static_cast<intptr_t>(texId)), InMin, InMax,
                                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 4.0f);
        } else {
            float midX = (InMin.x + InMax.x) * 0.5f;
            float midY = (InMin.y + InMax.y) * 0.5f;
            float iconHalf = (InMax.x - InMin.x) * 0.25f;
            FLucideIcons::DrawIcon(InDrawList, ImVec2(midX - iconHalf, midY - iconHalf),
                                   ImVec2(midX + iconHalf, midY + iconHalf), ELucideIcon::Image,
                                   IM_COL32(220, 100, 220, 255));
        }

        // Badge
        InDrawList->AddRectFilled(ImVec2(InMin.x + 3.0f, InMax.y - 13.0f), ImVec2(InMin.x + 23.0f, InMax.y - 3.0f),
                                  IM_COL32(180, 50, 180, 220), 2.0f);
        InDrawList->AddText(ImVec2(InMin.x + 5.0f, InMax.y - 14.0f), IM_COL32(255, 255, 255, 255), "TEX");
    }

    void FContentBrowserPanel::DrawMaterialThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax,
                                                     const fs::path& InPath) {
        float r = 0.35f, g = 0.70f, b = 0.95f;
        std::error_code ec;
        if (fs::exists(InPath, ec) && !ec) {
            std::ifstream file(InPath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (line.find("Albedo") != std::string::npos) {
                        size_t bracket = line.find('[');
                        if (bracket != std::string::npos) {
                            float pr = 1.0f, pg = 1.0f, pb = 1.0f;
                            if (sscanf_s(line.c_str() + bracket, "[%f, %f, %f", &pr, &pg, &pb) == 3) {
                                r = std::clamp(pr, 0.05f, 1.0f);
                                g = std::clamp(pg, 0.05f, 1.0f);
                                b = std::clamp(pb, 0.05f, 1.0f);
                            }
                        }
                    }
                }
            }
        }

        InDrawList->AddRectFilled(InMin, InMax, IM_COL32(20, 22, 26, 255), 4.0f);

        float centerX = (InMin.x + InMax.x) * 0.5f;
        float centerY = (InMin.y + InMax.y) * 0.5f;
        float radius = std::min(InMax.x - InMin.x, InMax.y - InMin.y) * 0.34f;

        InDrawList->AddEllipseFilled(ImVec2(centerX, centerY + radius * 0.75f), ImVec2(radius * 0.85f, radius * 0.25f),
                                     IM_COL32(5, 5, 8, 160));

        ImU32 baseColor =
            IM_COL32(static_cast<int>(r * 255.0f), static_cast<int>(g * 255.0f), static_cast<int>(b * 255.0f), 255);
        InDrawList->AddCircleFilled(ImVec2(centerX, centerY), radius, baseColor, 28);

        ImU32 shadowColor =
            IM_COL32(static_cast<int>(r * 60.0f), static_cast<int>(g * 60.0f), static_cast<int>(b * 60.0f), 190);
        InDrawList->AddCircleFilled(ImVec2(centerX + radius * 0.18f, centerY + radius * 0.18f), radius * 0.85f,
                                    shadowColor, 24);

        ImU32 diffuseColor =
            IM_COL32(static_cast<int>(std::min(255.0f, r * 280.0f)), static_cast<int>(std::min(255.0f, g * 280.0f)),
                     static_cast<int>(std::min(255.0f, b * 280.0f)), 140);
        InDrawList->AddCircleFilled(ImVec2(centerX - radius * 0.22f, centerY - radius * 0.22f), radius * 0.65f,
                                    diffuseColor, 24);

        InDrawList->AddCircleFilled(ImVec2(centerX - radius * 0.32f, centerY - radius * 0.32f), radius * 0.32f,
                                    IM_COL32(255, 255, 255, 180), 16);
        InDrawList->AddCircleFilled(ImVec2(centerX - radius * 0.35f, centerY - radius * 0.35f), radius * 0.15f,
                                    IM_COL32(255, 255, 255, 240), 12);

        InDrawList->AddCircle(ImVec2(centerX, centerY), radius, IM_COL32(255, 255, 255, 40), 28, 1.2f);

        InDrawList->AddRectFilled(ImVec2(InMin.x + 3.0f, InMax.y - 13.0f), ImVec2(InMin.x + 24.0f, InMax.y - 3.0f),
                                  IM_COL32(40, 150, 70, 220), 2.0f);
        InDrawList->AddText(ImVec2(InMin.x + 5.0f, InMax.y - 14.0f), IM_COL32(255, 255, 255, 255), "MAT");
    }

    void FContentBrowserPanel::DrawMeshThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax,
                                                 const fs::path& InPath) {
        (void)InPath;
        InDrawList->AddRectFilled(InMin, InMax, IM_COL32(22, 24, 28, 255), 4.0f);

        float centerX = (InMin.x + InMax.x) * 0.5f;
        float centerY = (InMin.y + InMax.y) * 0.5f;
        float size = std::min(InMax.x - InMin.x, InMax.y - InMin.y) * 0.32f;

        ImVec2 topC(centerX, centerY - size * 0.85f);
        ImVec2 topR(centerX + size * 0.82f, centerY - size * 0.35f);
        ImVec2 topL(centerX - size * 0.82f, centerY - size * 0.35f);
        ImVec2 midC(centerX, centerY + size * 0.15f);
        ImVec2 botL(centerX - size * 0.82f, centerY + size * 0.65f);
        ImVec2 botR(centerX + size * 0.82f, centerY + size * 0.65f);
        ImVec2 botC(centerX, centerY + size * 1.15f);

        InDrawList->AddEllipseFilled(ImVec2(centerX, centerY + size * 0.95f), ImVec2(size * 0.95f, size * 0.30f),
                                     IM_COL32(5, 5, 8, 140));

        ImVec2 topFace[4] = {topC, topR, midC, topL};
        InDrawList->AddConvexPolyFilled(topFace, 4, IM_COL32(95, 160, 240, 255));

        ImVec2 leftFace[4] = {topL, midC, botC, botL};
        InDrawList->AddConvexPolyFilled(leftFace, 4, IM_COL32(65, 120, 200, 255));

        ImVec2 rightFace[4] = {midC, topR, botR, botC};
        InDrawList->AddConvexPolyFilled(rightFace, 4, IM_COL32(45, 90, 165, 255));

        InDrawList->AddPolyline(topFace, 4, IM_COL32(180, 220, 255, 200), ImDrawFlags_Closed, 1.2f);
        InDrawList->AddPolyline(leftFace, 4, IM_COL32(140, 190, 245, 180), ImDrawFlags_Closed, 1.2f);
        InDrawList->AddPolyline(rightFace, 4, IM_COL32(120, 170, 235, 180), ImDrawFlags_Closed, 1.2f);

        InDrawList->AddRectFilled(ImVec2(InMin.x + 3.0f, InMax.y - 13.0f), ImVec2(InMin.x + 20.0f, InMax.y - 3.0f),
                                  IM_COL32(35, 110, 210, 220), 2.0f);
        InDrawList->AddText(ImVec2(InMin.x + 5.0f, InMax.y - 14.0f), IM_COL32(255, 255, 255, 255), "3D");
    }

    void FContentBrowserPanel::SetContentDirectory(const std::string& InContentDir) {
        DestroyThumbnails();
        std::error_code ec;
        if (!InContentDir.empty() && fs::exists(InContentDir, ec) && !ec) {
            BaseContentPath = fs::canonical(InContentDir, ec);
            if (ec) {
                BaseContentPath = fs::path(InContentDir);
            }
            CurrentDirectory = BaseContentPath;
            LastClickedPath.clear();
            History.clear();
            History.push_back(CurrentDirectory);
            HistoryIndex = 0;
        } else {
            BaseContentPath.clear();
            CurrentDirectory.clear();
            LastClickedPath.clear();
            History.clear();
            HistoryIndex = -1;
        }
    }

    void FContentBrowserPanel::NavigateTo(const fs::path& InDir) {
        std::error_code ec;
        if (InDir.empty() || !fs::exists(InDir, ec) || !fs::is_directory(InDir, ec) || ec) {
            return;
        }

        fs::path targetDir = fs::canonical(InDir, ec);
        if (ec) {
            targetDir = InDir;
        }

        if (targetDir == CurrentDirectory) {
            return;
        }

        CurrentDirectory = targetDir;
        LastClickedPath.clear();

        if (HistoryIndex >= 0 && HistoryIndex + 1 < static_cast<int>(History.size())) {
            History.erase(History.begin() + HistoryIndex + 1, History.end());
        }
        History.push_back(CurrentDirectory);
        HistoryIndex = static_cast<int>(History.size()) - 1;
    }

    void FContentBrowserPanel::NavigateBack() {
        if (HistoryIndex > 0) {
            --HistoryIndex;
            CurrentDirectory = History[HistoryIndex];
            LastClickedPath.clear();
        }
    }

    void FContentBrowserPanel::NavigateForward() {
        if (HistoryIndex >= 0 && HistoryIndex + 1 < static_cast<int>(History.size())) {
            ++HistoryIndex;
            CurrentDirectory = History[HistoryIndex];
            LastClickedPath.clear();
        }
    }

    void FContentBrowserPanel::NavigateUp() {
        std::error_code ec;
        if (CurrentDirectory != BaseContentPath && CurrentDirectory.has_parent_path()) {
            fs::path parent = CurrentDirectory.parent_path();
            if (fs::exists(parent, ec) && !ec) {
                NavigateTo(parent);
            }
        }
    }

    void FContentBrowserPanel::NavigateHome() {
        NavigateTo(BaseContentPath);
    }

    void FContentBrowserPanel::Draw(bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::ContentBrowser, bInOutOpen, ELucideIcon::Folder);

        try {
            std::error_code ec;
            if (BaseContentPath.empty() || !fs::exists(BaseContentPath, ec) || ec) {
                ImGui::TextDisabled("No active project loaded or Content folder missing.");
                ImGui::End();
                return;
            }

            if (!fs::exists(CurrentDirectory, ec) || ec) {
                CurrentDirectory = BaseContentPath;
            }

            DrawTopBar();

            ImGui::Columns(2, "ContentBrowserSplitLayout", true);
            ImGui::SetColumnWidth(0, 210.0f);

            ImGui::BeginChild("FolderTreeScrollRegion", ImVec2(0, 0), false);
            DrawDirectoryTree(BaseContentPath);
            ImGui::EndChild();

            ImGui::NextColumn();
            DrawAssetView();
            ImGui::Columns(1);

            DrawRenameModal();
            DrawDeleteModal();
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FContentBrowserPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Content Browser Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FContentBrowserPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Content Browser: Unknown error encountered");
        }

        ImGui::End();
    }

    void FContentBrowserPanel::DrawTopBar() {
        constexpr float kBtnH = 26.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        // Row 1: Add | Import | Save All | Back | Forward | [breadcrumb] | Settings
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Plus, "Add", "CB_Add", kBtnH)) {
            ImGui::OpenPopup("AddContentPopup");
        }
        if (ImGui::BeginPopup("AddContentPopup")) {
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Create Asset");
            ImGui::Separator();
            if (ImGui::MenuItem("Folder")) {
                CreateNewFolder();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Material (.lmat)")) {
                CreateNewAsset("Material");
            }
            if (ImGui::MenuItem("Level / Map (.lmap)")) {
                CreateNewAsset("Map");
            }
            if (ImGui::MenuItem("Blueprint / Script (.lua)")) {
                CreateNewAsset("Script");
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(0.0f, spacing);
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Download, "Import", "CB_Import", kBtnH)) {
            ImportExternalAsset();
        }

        ImGui::SameLine(0.0f, spacing);
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Save, "Save All", "CB_SaveAll", kBtnH)) {
            if (OnSaveAll) {
                OnSaveAll();
            }
        }

        ImGui::SameLine(0.0f, spacing * 1.5f);

        const bool bCanBack = (HistoryIndex > 0);
        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::ChevronLeft, "CB_Back", bCanBack, kBtnH)) {
            NavigateBack();
        }
        ImGui::SameLine(0.0f, 2.0f);

        const bool bCanForward = (HistoryIndex >= 0 && HistoryIndex + 1 < static_cast<int>(History.size()));
        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::ChevronRight, "CB_Forward", bCanForward, kBtnH)) {
            NavigateForward();
        }

        ImGui::SameLine(0.0f, spacing);

        const float settingsW = kBtnH;
        const float breadcrumbW = std::max(80.0f, ImGui::GetContentRegionAvail().x - settingsW - spacing);
        DrawBreadcrumbs(breadcrumbW, kBtnH);

        ImGui::SameLine(0.0f, spacing);
        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::Settings, "CB_Settings", true, kBtnH)) {
            ImGui::OpenPopup("ContentBrowserSettingsPopup");
        }
        if (ImGui::BeginPopup("ContentBrowserSettingsPopup")) {
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "View Settings");
            ImGui::Separator();
            FEditorWidgets::DrawSliderFloat("##CardSize", &CardSize, 60.0f, 160.0f, "%.0f px");
            ImGui::SameLine();
            ImGui::TextUnformatted("Card Size");
            FEditorWidgets::DrawCheckbox("##ShowExt", &bShowExtensions, "Show Extensions");

            ImGui::Spacing();
            ImGui::TextDisabled("View Mode");
            const char* viewModes[] = {"Grid", "List"};
            const ELucideIcon viewIcons[] = {ELucideIcon::LayoutGrid, ELucideIcon::FileText};
            int vm = (ViewMode == EContentBrowserViewMode::Grid) ? 0 : 1;
            if (FEditorWidgets::DrawSegmentedControl("##CBViewMode", &vm, viewIcons, viewModes, 2)) {
                ViewMode = (vm == 0) ? EContentBrowserViewMode::Grid : EContentBrowserViewMode::List;
            }
            ImGui::EndPopup();
        }
    }

    void FContentBrowserPanel::DrawBreadcrumbs(float InWidth, float InHeight) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.03f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::BeginChild("ContentBreadcrumbBar", ImVec2(InWidth, InHeight), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.24f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.45f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

        if (ImGui::SmallButton("Content")) {
            NavigateTo(BaseContentPath);
        }

        std::error_code ec;
        fs::path rel = fs::relative(CurrentDirectory, BaseContentPath, ec);
        if (!ec && rel != "." && !rel.empty()) {
            fs::path accumulated = BaseContentPath;
            for (const auto& part : rel) {
                accumulated /= part;
                ImGui::SameLine(0.0f, 2.0f);

                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 cur = ImGui::GetCursorScreenPos();
                const float chevronSize = 12.0f;
                FLucideIcons::DrawIcon(draw, cur, ImVec2(cur.x + chevronSize, cur.y + chevronSize),
                                       ELucideIcon::ChevronRight, IM_COL32(110, 115, 125, 255));
                ImGui::Dummy(ImVec2(chevronSize, chevronSize));
                ImGui::SameLine(0.0f, 2.0f);

                std::string segmentName = part.string();
                if (ImGui::SmallButton(segmentName.c_str())) {
                    NavigateTo(accumulated);
                }
            }
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void FContentBrowserPanel::DrawDirectoryTree(const fs::path& InDir) {
        std::error_code ec;
        if (!fs::exists(InDir, ec) || !fs::is_directory(InDir, ec) || ec) {
            return;
        }

        std::string pathStr = InDir.string();
        std::string folderName = (InDir == BaseContentPath) ? "Content" : InDir.filename().string();
        bool bIsCurrent = (fs::equivalent(InDir, CurrentDirectory, ec) && !ec);

        bool bHasSubdirs = false;
        for (const auto& entry : fs::directory_iterator(InDir, ec)) {
            if (ec)
                break;
            if (entry.is_directory(ec)) {
                bHasSubdirs = true;
                break;
            }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsCurrent) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!bHasSubdirs) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        } else {
            // Expand folder tree by default the first time each node is seen.
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        }

        ImGui::PushID(pathStr.c_str());

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 curPos = ImGui::GetCursorScreenPos();

        bool bOpen = ImGui::TreeNodeEx("##DirNode", flags, "   %s", folderName.c_str());

        ELucideIcon fIcon = (bOpen && bHasSubdirs) ? ELucideIcon::FolderOpen : ELucideIcon::Folder;
        ImU32 fColor = bIsCurrent ? IM_COL32(255, 210, 80, 255) : IM_COL32(230, 180, 60, 255);
        FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 18.0f, curPos.y + 2.0f),
                               ImVec2(curPos.x + 32.0f, curPos.y + 16.0f), fIcon, fColor);

        if (ImGui::IsItemClicked()) {
            NavigateTo(InDir);
        }

        // Folder Drag & Drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                std::string srcPathStr = static_cast<const char*>(payload->Data);
                fs::path src(srcPathStr);
                if (fs::exists(src, ec) && !ec) {
                    fs::path dst = InDir / src.filename();
                    fs::rename(src, dst, ec);
                    if (Context) {
                        Context->GetSelection().SelectAsset(dst.string());
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (bOpen && bHasSubdirs) {
            for (const auto& entry : fs::directory_iterator(InDir, ec)) {
                if (ec)
                    break;
                if (entry.is_directory(ec)) {
                    DrawDirectoryTree(entry.path());
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    std::string FContentBrowserPanel::GetAssetTypeString(const fs::path& InPath, bool bIsDir) const {
        if (bIsDir)
            return "Folder";
        std::string ext = InPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".lmat")
            return "Material";
        if (ext == ".lmap")
            return "Level";
        if (ext == ".lmesh")
            return "StaticMesh";
        if (ext == ".lskeletalmesh")
            return "SkeletalMesh";
        if (ext == ".lskeleton")
            return "Skeleton";
        if (ext == ".lanim")
            return "Animation";
        if (ext == ".ltex" || ext == ".lhdr")
            return "Texture";
        if (ext == ".obj" || ext == ".gltf" || ext == ".fbx")
            return "Source";
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".hdr")
            return "Texture";
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
            return "Audio";
        if (ext == ".lua")
            return "Script";
        return "Asset";
    }

    bool FContentBrowserPanel::PassesSearchFilter(const fs::directory_entry& InEntry,
                                                  const std::string& InQuery) const {
        if (InQuery.empty())
            return true;

        std::string lowerFilename = InEntry.path().filename().string();
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return lowerFilename.find(InQuery) != std::string::npos;
    }

    void FContentBrowserPanel::DrawAssetView() {
        std::error_code ec;
        if (!fs::exists(CurrentDirectory, ec) || ec)
            return;

        FEditorWidgets::DrawSearchInput("AssetSearchInput", SearchBuffer, sizeof(SearchBuffer), "Search by name...");

        ImGui::Spacing();

        std::string query = SearchBuffer;
        std::transform(query.begin(), query.end(), query.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Collect and sort directory entries
        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator(CurrentDirectory, ec)) {
            if (ec)
                break;
            std::string fn = entry.path().filename().string();
            if (fn.empty() || fn[0] == '.')
                continue;
            if (!PassesSearchFilter(entry, query))
                continue;
            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
            std::error_code e;
            bool aDir = a.is_directory(e);
            bool bDir = b.is_directory(e);
            if (aDir != bDir)
                return aDir > bDir; // Folders first
            return a.path().filename().string() < b.path().filename().string();
        });

        int totalCount = static_cast<int>(entries.size());
        int selectedCount = Context ? static_cast<int>(Context->GetSelection().GetSelectedAssetCount()) : 0;

        if (ViewMode == EContentBrowserViewMode::Grid) {
            DrawAssetGrid(entries);
        } else {
            DrawAssetList(entries);
        }

        // Delete key handler for selected assets in Content Browser
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::GetIO().WantTextInput) {
            if (Context && Context->GetSelection().GetSelectedAssetCount() > 0) {
                std::string firstAsset = Context->GetSelection().GetPrimarySelectedAsset();
                if (!firstAsset.empty()) {
                    RequestDeleteItem(firstAsset);
                }
            } else if (!LastClickedPath.empty()) {
                RequestDeleteItem(LastClickedPath);
            }
        }

        DrawFooter(totalCount, selectedCount);
    }

    void FContentBrowserPanel::DrawAssetGrid(const std::vector<fs::directory_entry>& InEntries) {
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(panelWidth / (CardSize + 16.0f)));

        ImGui::BeginChild("AssetGridChildRegion", ImVec2(0, -28.0f), false);

        int colIdx = 0;
        std::error_code ec;

        for (const auto& entry : InEntries) {
            std::string filename = entry.path().filename().string();
            std::string pathStr = entry.path().string();
            bool bIsDir = entry.is_directory(ec);
            std::string ext = entry.path().extension().string();
            std::string lowerExt = ext;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            bool bIsSelected = Context && Context->GetSelection().IsAssetSelected(pathStr);

            bool bIsTexture = (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".tga" ||
                               lowerExt == ".bmp" || lowerExt == ".hdr" || lowerExt == ".ltex");
            bool bIsMaterial = (lowerExt == ".lmat");
            bool bIsMesh = (lowerExt == ".lmesh" || lowerExt == ".lskeletalmesh");
            bool bIsMap = (lowerExt == ".lmap");
            bool bIsScript = (lowerExt == ".lua");
            bool bIsAudio = (lowerExt == ".wav" || lowerExt == ".ogg" || lowerExt == ".mp3");

            ImVec4 labelColor = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
            if (bIsDir)
                labelColor = ImVec4(0.95f, 0.80f, 0.35f, 1.0f);
            else if (bIsMap)
                labelColor = ImVec4(1.0f, 0.55f, 0.30f, 1.0f);
            else if (bIsMaterial)
                labelColor = ImVec4(0.40f, 0.90f, 0.50f, 1.0f);
            else if (bIsMesh)
                labelColor = ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
            else if (bIsTexture)
                labelColor = ImVec4(0.90f, 0.50f, 0.90f, 1.0f);
            else if (bIsScript)
                labelColor = ImVec4(0.55f, 0.85f, 1.0f, 1.0f);
            else if (bIsAudio)
                labelColor = ImVec4(1.0f, 0.70f, 0.35f, 1.0f);

            ImGui::PushID(filename.c_str());
            ImGui::BeginGroup();

            if (bIsSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.60f, 1.0f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.19f, 1.0f));
            }

            float cardHeight = CardSize * 0.75f;
            bool bClicked = ImGui::Button("##card_btn", ImVec2(CardSize, cardHeight));

            // Drag & Drop Source
            if (!bIsDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ASSET", pathStr.c_str(), pathStr.size() + 1);
                ImGui::Text("Asset: %s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            if (bIsSelected) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PopStyleColor();
            }

            ImVec2 btnMin = ImGui::GetItemRectMin();
            ImVec2 btnMax = ImGui::GetItemRectMax();
            ImDrawList* draw = ImGui::GetWindowDrawList();

            if (bIsMaterial) {
                DrawMaterialThumbnail(draw, ImVec2(btnMin.x + 2.0f, btnMin.y + 2.0f),
                                      ImVec2(btnMax.x - 2.0f, btnMax.y - 2.0f), entry.path());
            } else if (bIsMesh) {
                DrawMeshThumbnail(draw, ImVec2(btnMin.x + 2.0f, btnMin.y + 2.0f),
                                  ImVec2(btnMax.x - 2.0f, btnMax.y - 2.0f), entry.path());
            } else if (bIsTexture) {
                DrawTextureThumbnail(draw, ImVec2(btnMin.x + 2.0f, btnMin.y + 2.0f),
                                     ImVec2(btnMax.x - 2.0f, btnMax.y - 2.0f), entry.path());
            } else {
                ELucideIcon icon = ELucideIcon::FileText;
                ImU32 iconColor = IM_COL32(180, 180, 190, 255);
                if (bIsDir) {
                    icon = ELucideIcon::Folder;
                    iconColor = IM_COL32(245, 195, 65, 255);
                } else if (bIsMap) {
                    icon = ELucideIcon::Map;
                    iconColor = IM_COL32(255, 110, 60, 255);
                } else if (bIsScript) {
                    icon = ELucideIcon::FileCode;
                    iconColor = IM_COL32(100, 200, 255, 255);
                } else if (bIsAudio) {
                    icon = ELucideIcon::Activity;
                    iconColor = IM_COL32(255, 170, 70, 255);
                }
                float iconHalf = CardSize * 0.22f;
                float midX = (btnMin.x + btnMax.x) * 0.5f;
                float midY = (btnMin.y + btnMax.y) * 0.5f;
                FLucideIcons::DrawIcon(draw, ImVec2(midX - iconHalf, midY - iconHalf),
                                       ImVec2(midX + iconHalf, midY + iconHalf), icon, iconColor);
            }

            // Click handling with Ctrl and Shift multi-select
            if (bClicked || ImGui::IsItemClicked(0)) {
                LastClickedPath = entry.path();
                bool bCtrl = ImGui::GetIO().KeyCtrl;
                if (Context) {
                    if (bCtrl) {
                        Context->GetSelection().ToggleAssetSelection(pathStr);
                    } else {
                        Context->GetSelection().SelectAsset(pathStr, false);
                    }
                }
            }

            // Double Click: Open / Navigate
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                OpenAsset(entry.path());
            }

            // Context Menu
            if (ImGui::BeginPopupContextItem("AssetGridCardContext")) {
                if (Context) {
                    Context->GetSelection().SelectAsset(pathStr, false);
                }

                ImGui::TextDisabled("%s", filename.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("Open")) {
                    OpenAsset(entry.path());
                }

                if (!bIsDir && ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                    DuplicateAsset(entry.path());
                }

                if (ImGui::MenuItem("Rename", "F2")) {
                    bRenamingItem = true;
                    RenameTargetPath = entry.path();
                    strncpy_s(RenameBuffer, filename.c_str(), sizeof(RenameBuffer));
                }

                if (ImGui::MenuItem("Show in Explorer")) {
                    OpenInExplorer(entry.path());
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) {
                    RequestDeleteItem(entry.path());
                }

                ImGui::EndPopup();
            }

            std::string displayLabel = bShowExtensions ? filename : entry.path().stem().string();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + CardSize);
            ImGui::TextColored(labelColor, "%s", displayLabel.c_str());
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();

            colIdx++;
            if (colIdx % columns != 0) {
                ImGui::SameLine();
            }

            ImGui::PopID();
        }

        // Empty background context menu
        if (ImGui::BeginPopupContextWindow("AssetGridBackgroundContext",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("New Folder")) {
                CreateNewFolder();
            }
            if (ImGui::MenuItem("Import Asset...")) {
                ImportExternalAsset();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show in Explorer")) {
                OpenInExplorer(CurrentDirectory);
            }
            ImGui::EndPopup();
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
            if (Context) {
                Context->GetSelection().ClearAssetSelection();
            }
        }

        ImGui::EndChild();
    }

    void FContentBrowserPanel::DrawAssetList(const std::vector<fs::directory_entry>& InEntries) {
        ImGui::BeginChild("AssetListChildRegion", ImVec2(0, -28.0f), false);

        if (ImGui::BeginTable("AssetListTable", 4,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();

            std::error_code ec;
            for (const auto& entry : InEntries) {
                std::string filename = entry.path().filename().string();
                std::string pathStr = entry.path().string();
                bool bIsDir = entry.is_directory(ec);
                bool bIsSelected = Context && Context->GetSelection().IsAssetSelected(pathStr);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                ImGui::PushID(filename.c_str());

                // Selectable row
                if (ImGui::Selectable(filename.c_str(), bIsSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    bool bCtrl = ImGui::GetIO().KeyCtrl;
                    if (Context) {
                        if (bCtrl) {
                            Context->GetSelection().ToggleAssetSelection(pathStr);
                        } else {
                            Context->GetSelection().SelectAsset(pathStr, false);
                        }
                    }
                }

                // Drag & Drop Source
                if (!bIsDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("CONTENT_BROWSER_ASSET", pathStr.c_str(), pathStr.size() + 1);
                    ImGui::Text("Asset: %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    OpenAsset(entry.path());
                }

                // Context Menu
                if (ImGui::BeginPopupContextItem("AssetListRowContext")) {
                    if (Context)
                        Context->GetSelection().SelectAsset(pathStr, false);
                    ImGui::TextDisabled("%s", filename.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Open"))
                        OpenAsset(entry.path());
                    if (!bIsDir && ImGui::MenuItem("Duplicate", "Ctrl+D"))
                        DuplicateAsset(entry.path());
                    if (ImGui::MenuItem("Rename", "F2")) {
                        bRenamingItem = true;
                        RenameTargetPath = entry.path();
                        strncpy_s(RenameBuffer, filename.c_str(), sizeof(RenameBuffer));
                    }
                    if (ImGui::MenuItem("Show in Explorer"))
                        OpenInExplorer(entry.path());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", "Del"))
                        RequestDeleteItem(entry.path());
                    ImGui::EndPopup();
                }

                // Column 1: Type
                ImGui::TableSetColumnIndex(1);
                std::string typeStr = GetAssetTypeString(entry.path(), bIsDir);
                ImGui::TextUnformatted(typeStr.c_str());

                // Column 2: Size
                ImGui::TableSetColumnIndex(2);
                if (bIsDir) {
                    ImGui::TextDisabled("--");
                } else {
                    auto sz = entry.file_size(ec);
                    if (!ec) {
                        if (sz < 1024)
                            ImGui::Text("%llu B", sz);
                        else if (sz < 1024 * 1024)
                            ImGui::Text("%.1f KB", sz / 1024.0f);
                        else
                            ImGui::Text("%.2f MB", sz / (1024.0f * 1024.0f));
                    } else {
                        ImGui::TextDisabled("--");
                    }
                }

                // Column 3: Modified Date
                ImGui::TableSetColumnIndex(3);
                auto ftime = entry.last_write_time(ec);
                if (!ec) {
                    ImGui::TextDisabled("Recently");
                } else {
                    ImGui::TextDisabled("--");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    void FContentBrowserPanel::DrawFooter(int InTotalItems, int InSelectedCount) {
        ImGui::Separator();
        ImGui::Spacing();

        if (InSelectedCount > 0 && Context) {
            std::string primary = Context->GetSelection().GetPrimarySelectedAsset();
            std::string selName = fs::path(primary).filename().string();
            ImGui::TextDisabled("%d Assets  |  %d Selected (%s)", InTotalItems, InSelectedCount, selName.c_str());
        } else {
            ImGui::TextDisabled("%d Assets", InTotalItems);
        }
    }

    void FContentBrowserPanel::CreateNewFolder() {
        std::error_code ec;
        if (!fs::exists(CurrentDirectory, ec) || ec)
            return;

        int counter = 1;
        fs::path newPath = CurrentDirectory / "NewFolder";
        while (fs::exists(newPath, ec)) {
            newPath = CurrentDirectory / ("NewFolder_" + std::to_string(counter++));
        }

        fs::create_directory(newPath, ec);
        if (!ec && Context) {
            Context->GetSelection().SelectAsset(newPath.string());
        }
    }

    void FContentBrowserPanel::CreateNewAsset(const std::string& InAssetType) {
        std::error_code ec;
        if (!fs::exists(CurrentDirectory, ec) || ec)
            return;

        fs::path newAssetPath;
        if (InAssetType == "Material") {
            newAssetPath = CurrentDirectory / "M_NewMaterial.lmat";
            std::ofstream out(newAssetPath);
            if (out.is_open()) {
                out << "{\n  \"Shader\": \"DefaultPBR\",\n  \"Albedo\": [0.8, 0.2, 0.3],\n  \"Roughness\": 0.4,\n  "
                       "\"Metallic\": 0.1\n}\n";
            }
        } else if (InAssetType == "Map") {
            newAssetPath = CurrentDirectory / "NewLevel.lmap";
            std::ofstream out(newAssetPath);
            if (out.is_open()) {
                out << "{\n  \"MapName\": \"NewLevel\",\n  \"Actors\": []\n}\n";
            }
        } else if (InAssetType == "Script") {
            newAssetPath = CurrentDirectory / "NewScript.lua";
            std::ofstream out(newAssetPath);
            if (out.is_open()) {
                out << "-- LeonEngine Lua Script Component\nfunction OnBeginPlay()\nend\n\nfunction "
                       "OnUpdate(dt)\nend\n";
            }
        }

        if (!newAssetPath.empty() && Context) {
            Context->GetSelection().SelectAsset(newAssetPath.string());
        }
    }

    void FContentBrowserPanel::CopyNativeAssetIntoContent(const fs::path& InSrc) {
        std::error_code ec;
        if (!fs::exists(CurrentDirectory, ec) || ec) {
            LE_CORE_ERROR("FContentBrowserPanel: Content directory is invalid");
            if (OnNotify)
                OnNotify("Cannot import: Content folder is invalid", true);
            return;
        }

        fs::path dst = CurrentDirectory / InSrc.filename();
        fs::copy_file(InSrc, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            LE_CORE_ERROR("FContentBrowserPanel: Failed to copy '{}' to '{}': {}", InSrc.string(), dst.string(),
                          ec.message());
            if (OnNotify)
                OnNotify("Failed to copy " + InSrc.filename().string() + " into Content", true);
            return;
        }

        LE_CORE_INFO("FContentBrowserPanel: Copied native asset to '{}'", dst.string());
        if (OnNotify)
            OnNotify("Copied " + InSrc.filename().string() + " into Content", false);
        if (Context)
            Context->GetSelection().SelectAsset(dst.string());
    }

    void FContentBrowserPanel::ImportExternalAsset() {
        std::error_code ec;
        if (!fs::exists(CurrentDirectory, ec) || ec)
            return;

        if (OnQueryImportBusy && OnQueryImportBusy()) {
            if (OnNotify)
                OnNotify("An import is already running", true);
            return;
        }

        std::string selectedFile = FEditorFileDialog::OpenFile(
            "All Supported Assets "
            "(*.fbx;*.obj;*.png;*.jpg;*.tga;*.hdr;*.wav;*.ogg;*.lmesh;*.ltex)\0"
            "*.fbx;*.obj;*.png;*.jpg;*.tga;*.hdr;*.wav;*.ogg;*.lmesh;*.ltex\0"
            "3D Models (*.fbx;*.obj)\0*.fbx;*.obj\0"
            "Textures (*.png;*.jpg;*.tga;*.hdr)\0*.png;*.jpg;*.tga;*.hdr\0"
            "Audio (*.wav;*.ogg;*.mp3)\0*.wav;*.ogg;*.mp3\0"
            "Native Leon Assets "
            "(*.lmesh;*.lskeletalmesh;*.lskeleton;*.lanim;*.ltex;*.lhdr;*.lmat;*.lmap)\0"
            "*.lmesh;*.lskeletalmesh;*.lskeleton;*.lanim;*.ltex;*.lhdr;*.lmat;*.lmap\0"
            "All Files (*.*)\0*.*\0",
            "Import Asset");

        if (selectedFile.empty() || !fs::exists(selectedFile, ec) || ec)
            return;

        fs::path src(selectedFile);
        std::string ext = src.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const bool bNativeCopy = (ext == ".lmesh" || ext == ".lskeletalmesh" || ext == ".lskeleton" ||
                                  ext == ".lanim" || ext == ".ltex" || ext == ".lhdr" || ext == ".lmat" ||
                                  ext == ".lmap" || ext == ".wav" || ext == ".ogg" || ext == ".mp3");
        const bool bAssetToolSource = (ext == ".fbx" || ext == ".obj" || ext == ".png" || ext == ".jpg" ||
                                       ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr" ||
                                       ext == ".exr");

        if (bNativeCopy) {
            CopyNativeAssetIntoContent(src);
            return;
        }

        if (ext == ".gltf" || ext == ".glb") {
            LE_CORE_WARN("FContentBrowserPanel: GLTF import is not supported; use FBX/OBJ or a native .lmesh");
            if (OnNotify)
                OnNotify("GLTF is not supported. Import FBX/OBJ via AssetTool, or copy a native .lmesh", true);
            return;
        }

        if (!bAssetToolSource) {
            LE_CORE_WARN("FContentBrowserPanel: Unsupported import extension '{}'", ext);
            if (OnNotify)
                OnNotify("Unsupported import format: " + ext, true);
            return;
        }

        if (!OnImportAsset) {
            LE_CORE_ERROR("FContentBrowserPanel: Import pipeline is not wired");
            if (OnNotify)
                OnNotify("Import pipeline is not available", true);
            return;
        }

        OnImportAsset(src.string());
    }

    void FContentBrowserPanel::DuplicateAsset(const fs::path& InPath) {
        std::error_code ec;
        if (!fs::exists(InPath, ec) || ec)
            return;

        std::string stem = InPath.stem().string();
        std::string ext = InPath.extension().string();
        int counter = 1;
        fs::path copyPath = InPath.parent_path() / (stem + "_Copy" + ext);
        while (fs::exists(copyPath, ec)) {
            copyPath = InPath.parent_path() / (stem + "_Copy" + std::to_string(++counter) + ext);
        }

        fs::copy_file(InPath, copyPath, ec);
        if (!ec && Context) {
            Context->GetSelection().SelectAsset(copyPath.string());
        }
    }

    void FContentBrowserPanel::OpenAsset(const fs::path& InPath) {
        std::error_code ec;
        if (fs::is_directory(InPath, ec)) {
            NavigateTo(InPath);
            return;
        }

        std::string ext = InPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".lmap") {
            if (OnMapSelected) {
                OnMapSelected(InPath.string());
            }
        }
    }

    void FContentBrowserPanel::OpenInExplorer(const fs::path& InPath) {
        std::error_code ec;
        if (!fs::exists(InPath, ec) || ec)
            return;

        std::string cmd;
#ifdef _WIN32
        if (fs::is_directory(InPath, ec)) {
            cmd = "explorer.exe \"" + InPath.string() + "\"";
        } else {
            cmd = "explorer.exe /select,\"" + InPath.string() + "\"";
        }
        std::system(cmd.c_str());
#endif
    }

    void FContentBrowserPanel::DeleteItem(const fs::path& InPath) {
        std::error_code ec;
        if (!fs::exists(InPath, ec) || ec)
            return;

        if (fs::is_directory(InPath, ec)) {
            fs::remove_all(InPath, ec);
        } else {
            fs::remove(InPath, ec);
        }

        if (Context) {
            Context->GetSelection().DeselectAsset(InPath.string());
        }
    }

    void FContentBrowserPanel::RequestDeleteItem(const fs::path& InPath) {
        DeleteTargetPath = InPath;
        CachedDeleteReferences = FindAssetReferencesInWorld(InPath);
        bConfirmingDelete = true;
    }

    std::vector<FContentBrowserPanel::FAssetReferenceInfo> FContentBrowserPanel::FindAssetReferencesInWorld(const fs::path& InPath) {
        std::vector<FAssetReferenceInfo> refs;
        if (!Context || !Context->GetActiveWorld()) {
            return refs;
        }

        auto PathsReferToSameAsset = [](const std::string& InA, const std::string& InB) -> bool {
            if (InA.empty() || InB.empty())
                return false;
            const std::string a = FAssetPath::Normalize(InA);
            const std::string b = FAssetPath::Normalize(InB);
            if (a == b)
                return true;
            const std::string va = FProjectPaths::MakeVirtualPath(a);
            const std::string vb = FProjectPaths::MakeVirtualPath(b);
            if (!va.empty() && va == b)
                return true;
            if (!vb.empty() && vb == a)
                return true;
            if (!va.empty() && !vb.empty() && va == vb)
                return true;
            return false;
        };

        UWorld* world = Context->GetActiveWorld();
        const std::string pathStr = FAssetPath::Normalize(InPath.string());

        for (const auto& actorPtr : world->GetAllActors()) {
            AActor* actor = actorPtr.get();
            if (!actor)
                continue;

            if (actor->HasComponent<FStaticMeshComponent>()) {
                const auto& smc = actor->GetComponent<FStaticMeshComponent>();
                if (PathsReferToSameAsset(pathStr, smc.AssetPath)) {
                    refs.push_back({actor->GetName(), "StaticMeshComponent", actor});
                }

                for (const auto& matPath : smc.MaterialOverridePaths) {
                    if (PathsReferToSameAsset(pathStr, matPath)) {
                        refs.push_back({actor->GetName(), "StaticMesh (Material Override)", actor});
                        break;
                    }
                }
            }

            if (actor->HasComponent<FMaterialComponent>()) {
                const auto& mat = actor->GetComponent<FMaterialComponent>();
                if (PathsReferToSameAsset(pathStr, mat.AssetPath)) {
                    refs.push_back({actor->GetName(), "MaterialComponent", actor});
                }
            }
        }

        return refs;
    }

    void FContentBrowserPanel::UnlinkAssetReferences(const fs::path& InPath, const std::vector<FAssetReferenceInfo>& InRefs) {
        (void)InPath;
        for (const auto& ref : InRefs) {
            if (!ref.ActorPtr) continue;

            if (ref.ActorPtr->HasComponent<FStaticMeshComponent>()) {
                auto& smc = ref.ActorPtr->GetComponent<FStaticMeshComponent>();
                smc.StaticMesh = nullptr;
                smc.AssetPath.clear();
                smc.MaterialOverrides.clear();
                smc.MaterialOverridePaths.clear();
            }
        }
    }

    void FContentBrowserPanel::DrawDeleteModal() {
        if (bConfirmingDelete) {
            ImGui::OpenPopup("Delete Asset / Folder");
            bConfirmingDelete = false;
        }

        if (ImGui::BeginPopupModal("Delete Asset / Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::string itemName = DeleteTargetPath.filename().string();
            std::error_code ec;
            bool bIsDir = fs::is_directory(DeleteTargetPath, ec);

            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Delete %s:", bIsDir ? "Folder" : "Asset");
            ImGui::SameLine();
            ImGui::TextUnformatted(itemName.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (CachedDeleteReferences.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "No active actor references in loaded level.");
                ImGui::TextDisabled("This item will be deleted from disk.");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                   "Found %zu active reference(s) in the current level:", CachedDeleteReferences.size());
                ImGui::TextDisabled("Confirming will unlink the asset from referencing actors before deleting.");

                ImGui::Spacing();
                float boxHeight = std::min(130.0f, 24.0f * (static_cast<float>(CachedDeleteReferences.size()) + 1.0f));
                ImGui::BeginChild("ReferenceListScrollBox", ImVec2(380.0f, boxHeight), true);
                for (const auto& ref : CachedDeleteReferences) {
                    ImGui::BulletText("%s (%s)", ref.ActorName.c_str(), ref.ComponentName.c_str());
                }
                ImGui::EndChild();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Confirm Delete button (Red)
            {
                FControlStyle delStyle;
                delStyle.bOverrideAccent = true;
                delStyle.Accent = FEditorTheme::GetTokens().Destructive;
                const char* delLabel =
                    CachedDeleteReferences.empty() ? "Delete" : "Delete & Unlink References";
                const float delW = CachedDeleteReferences.empty() ? 120.0f : 230.0f;
                bool bConfirmed = FEditorWidgets::DrawPrimaryButton(ELucideIcon::Trash, "##ConfirmDelete", delLabel,
                                                                    ImVec2(delW, 28.0f), &delStyle);

                if (bConfirmed || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    if (!CachedDeleteReferences.empty()) {
                        UnlinkAssetReferences(DeleteTargetPath, CachedDeleteReferences);
                    }
                    DeleteItem(DeleteTargetPath);
                    CachedDeleteReferences.clear();
                    DeleteTargetPath.clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::X, "##CancelDelete", "Cancel", ImVec2(100.0f, 28.0f)) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                CachedDeleteReferences.clear();
                DeleteTargetPath.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void FContentBrowserPanel::DrawRenameModal() {
        if (bRenamingItem) {
            ImGui::OpenPopup("Rename Asset / Folder");
            bRenamingItem = false;
        }

        if (ImGui::BeginPopupModal("Rename Asset / Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter new name:");
            ImGui::SetNextItemWidth(300.0f);
            bool bEnterPressed = ImGui::InputText("##NewNameInput", RenameBuffer, sizeof(RenameBuffer),
                                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto PerformRename = [this]() {
                if (RenameBuffer[0] != '\0' && !RenameTargetPath.empty()) {
                    std::error_code dirEc;
                    const bool bIsDir = fs::is_directory(RenameTargetPath, dirEc);
                    if (!bIsDir) {
                        const auto refs = FindAssetReferencesInWorld(RenameTargetPath);
                        if (!refs.empty()) {
                            LE_CORE_WARN(
                                "FContentBrowserPanel: Rename of '{}' blocked: {} actor reference(s) in the loaded map",
                                RenameTargetPath.string(), refs.size());
                            if (OnNotify)
                                OnNotify("Cannot rename: asset is referenced by actors in the loaded map", true);
                            ImGui::CloseCurrentPopup();
                            return;
                        }
                    }

                    std::error_code ec;
                    fs::path newPath = RenameTargetPath.parent_path() / RenameBuffer;
                    if (!fs::exists(newPath, ec) && !ec) {
                        fs::rename(RenameTargetPath, newPath, ec);
                        if (!ec && Context) {
                            Context->GetSelection().SelectAsset(newPath.string(), false);
                        }
                    }
                }
                ImGui::CloseCurrentPopup();
            };

            if (FEditorWidgets::DrawPrimaryButton(ELucideIcon::Pencil, "##RenameConfirm", "Rename", ImVec2(120.0f, 0)) ||
                bEnterPressed) {
                PerformRename();
            }

            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::X, "##RenameCancel", "Cancel", ImVec2(120.0f, 0)) ||
                ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Leon::Editor
