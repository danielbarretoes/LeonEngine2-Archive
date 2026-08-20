#pragma once

#include "Core/Base.hpp"
#include "Editor/Subsystems/FEditorSelectionSubsystem.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace Leon::Editor {

    enum class EContentBrowserViewMode { Grid, List };

    /**
     * @brief Professional Unreal Engine–inspired Content Browser panel with advanced search filters,
     * Grid and List view modes, dynamic Add/Create menus, asset import pipeline, interactive breadcrumbs,
     * and integration with FEditorSelectionSubsystem.
     */
    class FContentBrowserPanel {
    public:
        using FOnMapSelected = std::function<void(const std::string& InMapPath)>;
        using FOnSaveAll = std::function<void()>;

        FContentBrowserPanel() = default;
        ~FContentBrowserPanel();

        void SetContentDirectory(const std::string& InContentDir);
        void SetSelectionSubsystem(FEditorSelectionSubsystem* InSubsystem) { SelectionSubsystem = InSubsystem; }
        void SetOnMapSelected(FOnMapSelected InCallback) { OnMapSelected = std::move(InCallback); }
        void SetOnSaveAll(FOnSaveAll InCallback) { OnSaveAll = std::move(InCallback); }

        void Draw(bool* bInOutOpen = nullptr);

    private:
        void DrawTopBar();
        void DrawBreadcrumbs();
        void DrawDirectoryTree(const std::filesystem::path& InDir);
        void DrawAssetView();
        void DrawAssetGrid(const std::vector<std::filesystem::directory_entry>& InEntries);
        void DrawAssetList(const std::vector<std::filesystem::directory_entry>& InEntries);
        void DrawFooter(int InTotalItems, int InSelectedCount);

        void DrawTextureThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax,
                                  const std::filesystem::path& InPath);
        void DrawMaterialThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax,
                                   const std::filesystem::path& InPath);
        void DrawMeshThumbnail(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax, const std::filesystem::path& InPath);

        uint32_t GetOrCreateTextureThumbnail(const std::string& InPath);
        void DestroyThumbnails();

        // Navigation
        void NavigateTo(const std::filesystem::path& InDir);
        void NavigateBack();
        void NavigateForward();
        void NavigateUp();
        void NavigateHome();

        // Operations
        void CreateNewFolder();
        void CreateNewAsset(const std::string& InAssetType);
        void ImportExternalAsset();
        void OpenInExplorer(const std::filesystem::path& InPath);
        void DeleteItem(const std::filesystem::path& InPath);
        void DuplicateAsset(const std::filesystem::path& InPath);
        void OpenAsset(const std::filesystem::path& InPath);

        bool PassesSearchFilter(const std::filesystem::directory_entry& InEntry, const std::string& InQuery) const;
        std::string GetAssetTypeString(const std::filesystem::path& InPath, bool bIsDir) const;

        std::filesystem::path BaseContentPath;
        std::filesystem::path CurrentDirectory;
        std::filesystem::path LastClickedPath;

        // Navigation History
        std::vector<std::filesystem::path> History;
        int HistoryIndex = -1;

        // Subsystems and Callbacks
        FEditorSelectionSubsystem* SelectionSubsystem = nullptr;
        FOnMapSelected OnMapSelected;
        FOnSaveAll OnSaveAll;

        // Settings & Filters
        char SearchBuffer[128] = "";
        float CardSize = 92.0f;
        EContentBrowserViewMode ViewMode = EContentBrowserViewMode::Grid;
        bool bShowExtensions = true;

        // Renaming
        bool bRenamingItem = false;
        char RenameBuffer[128] = "";
        std::filesystem::path RenameTargetPath;

        // Thumbnail Cache (filepath -> OpenGL Texture ID)
        std::unordered_map<std::string, uint32_t> TextureThumbnailCache;
    };

} // namespace Leon::Editor
