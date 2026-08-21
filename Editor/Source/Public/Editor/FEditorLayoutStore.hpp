#pragma once

#include <string>
#include <vector>

namespace Leon::Editor {

    /** Panel visibility snapshot stored with a named dock layout. */
    struct FEditorPanelVisibility {
        bool bShowViewport = true;
        bool bShowPlaceActors = true;
        bool bShowOutliner = true;
        bool bShowDetails = true;
        bool bShowContentBrowser = true;
        bool bShowOutputLog = true;
        bool bShowWorldSettings = true;
        bool bShowProjectSettings = true;
    };

    /**
     * Named ImGui dock layouts under Editor/Saved/Layouts/.
     * Each layout is an imgui.ini snapshot plus a small panels meta file.
     * Empty ActiveLayoutName = shipped default (Editor/Resources/Layouts/Default.*).
     */
    class FEditorLayoutStore {
    public:
        void Init(const std::string& InEditorSavedDir);

        [[nodiscard]] const std::string& GetActiveLayoutName() const { return ActiveLayoutName; }
        [[nodiscard]] const std::vector<std::string>& GetLayoutNames() const { return LayoutNames; }

        void RefreshLayoutList();
        void SetActiveLayoutName(const std::string& InName);
        void ClearActiveLayout();

        /** Returns sanitized name, or empty if invalid. */
        static std::string SanitizeLayoutName(const std::string& InName);

        bool SaveNamedLayout(const std::string& InName, const FEditorPanelVisibility& InPanels);
        bool LoadNamedLayout(const std::string& InName, FEditorPanelVisibility& OutPanels);
        bool DeleteNamedLayout(const std::string& InName);

        /**
         * Load the shipped default dock layout from Editor/Resources/Layouts/Default.ini
         * (does not create a user named layout). Clears ActiveLayoutName.
         */
        bool LoadBundledDefaultLayout(FEditorPanelVisibility& OutPanels);

        [[nodiscard]] std::string GetLayoutIniPath(const std::string& InName) const;

    private:
        void LoadActivePreference();
        void SaveActivePreference() const;
        [[nodiscard]] std::string GetLayoutMetaPath(const std::string& InName) const;
        /** Absolute or relative path to directory containing Default.ini, or empty. */
        [[nodiscard]] static std::string FindBundledDefaultDir();
        static bool WritePanelMeta(const std::string& InPath, const std::string& InName,
                                   const FEditorPanelVisibility& InPanels);
        static bool ReadPanelMeta(const std::string& InPath, FEditorPanelVisibility& OutPanels);

        std::string EditorSavedDir;
        std::string LayoutsDir;
        std::string ActiveLayoutName;
        std::vector<std::string> LayoutNames;
    };

} // namespace Leon::Editor
