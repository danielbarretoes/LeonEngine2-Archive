#pragma once

#include "Core/Base.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace Leon::Editor {

    /**
     * @brief Content Browser panel for navigating project Content/ directory.
     */
    class FContentBrowserPanel {
    public:
        using FOnMapSelected = std::function<void(const std::string& InMapPath)>;

        FContentBrowserPanel() = default;

        void SetContentDirectory(const std::string& InContentDir);
        void SetOnMapSelected(FOnMapSelected InCallback) { OnMapSelected = std::move(InCallback); }

        void Draw();

    private:
        void DrawDirectoryTree(const std::filesystem::path& InDir);
        void DrawAssetGrid();

        std::filesystem::path BaseContentPath;
        std::filesystem::path CurrentDirectory;
        FOnMapSelected OnMapSelected;
        char SearchBuffer[128] = "";
    };

} // namespace Leon::Editor
