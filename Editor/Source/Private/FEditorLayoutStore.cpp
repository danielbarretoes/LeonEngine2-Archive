#include "Editor/FEditorLayoutStore.hpp"

#include "Core/FLog.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace Leon::Editor {

    namespace {

        std::string EscapeJson(const std::string& In) {
            std::string Out;
            Out.reserve(In.size());
            for (char c : In) {
                if (c == '\\' || c == '"')
                    Out.push_back('\\');
                Out.push_back(c);
            }
            return Out;
        }

        bool ParseBoolAfterKey(const std::string& Content, const char* Key, bool& OutValue) {
            const std::string needle = std::string("\"") + Key + "\"";
            size_t pos = Content.find(needle);
            if (pos == std::string::npos)
                return false;
            pos = Content.find(':', pos + needle.size());
            if (pos == std::string::npos)
                return false;
            while (pos + 1 < Content.size() && (Content[pos + 1] == ' ' || Content[pos + 1] == '\t'))
                ++pos;
            if (Content.compare(pos + 1, 4, "true") == 0) {
                OutValue = true;
                return true;
            }
            if (Content.compare(pos + 1, 5, "false") == 0) {
                OutValue = false;
                return true;
            }
            return false;
        }

    } // namespace

    void FEditorLayoutStore::Init(const std::string& InEditorSavedDir) {
        EditorSavedDir = InEditorSavedDir;
        LayoutsDir = (fs::path(EditorSavedDir) / "Layouts").string();
        std::error_code Ec;
        fs::create_directories(LayoutsDir, Ec);
        LoadActivePreference();
        // Drop stale preference pointing at the old user "Main" layout (now bundled as Default).
        if (ActiveLayoutName == "Main" || ActiveLayoutName == "main")
            ClearActiveLayout();
        RefreshLayoutList();
    }

    std::string FEditorLayoutStore::FindBundledDefaultDir() {
        const std::vector<fs::path> Candidates = {
            fs::path("Editor") / "Resources" / "Layouts",
            fs::path("Resources") / "Layouts",
            fs::path("..") / ".." / "Editor" / "Resources" / "Layouts",
        };
        std::error_code Ec;
        for (const auto& Dir : Candidates) {
            if (fs::exists(Dir / "Default.ini", Ec))
                return Dir.string();
        }
        return {};
    }

    bool FEditorLayoutStore::LoadBundledDefaultLayout(FEditorPanelVisibility& OutPanels) {
        const std::string Dir = FindBundledDefaultDir();
        if (Dir.empty()) {
            LE_CORE_WARN("FEditorLayoutStore: Bundled Default.ini not found under Editor/Resources/Layouts");
            return false;
        }

        const fs::path IniPath = fs::path(Dir) / "Default.ini";
        const fs::path MetaPath = fs::path(Dir) / "Default.panels.json";
        std::error_code Ec;
        if (!fs::exists(IniPath, Ec))
            return false;

        OutPanels = FEditorPanelVisibility{};
        if (fs::exists(MetaPath, Ec))
            ReadPanelMeta(MetaPath.string(), OutPanels);

        ImGui::LoadIniSettingsFromDisk(IniPath.string().c_str());
        ClearActiveLayout();
        LE_CORE_INFO("FEditorLayoutStore: Loaded bundled default layout from '{}'", IniPath.string());
        return true;
    }

    void FEditorLayoutStore::RefreshLayoutList() {
        LayoutNames.clear();
        std::error_code Ec;
        if (!fs::exists(LayoutsDir, Ec))
            return;

        for (const auto& Entry : fs::directory_iterator(LayoutsDir, Ec)) {
            if (Ec || !Entry.is_regular_file())
                continue;
            if (Entry.path().extension() != ".ini")
                continue;
            const std::string stem = Entry.path().stem().string();
            if (stem.empty() || stem == "ActiveLayout")
                continue;
            // Prefer entries that also have meta; still list orphan .ini files.
            LayoutNames.push_back(stem);
        }
        std::sort(LayoutNames.begin(), LayoutNames.end(),
                  [](const std::string& A, const std::string& B) {
                      return std::lexicographical_compare(
                          A.begin(), A.end(), B.begin(), B.end(),
                          [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) <
                                                     std::tolower(static_cast<unsigned char>(b)); });
                  });
    }

    void FEditorLayoutStore::SetActiveLayoutName(const std::string& InName) {
        ActiveLayoutName = InName;
        SaveActivePreference();
    }

    void FEditorLayoutStore::ClearActiveLayout() {
        ActiveLayoutName.clear();
        SaveActivePreference();
    }

    std::string FEditorLayoutStore::SanitizeLayoutName(const std::string& InName) {
        std::string Out;
        Out.reserve(InName.size());
        for (char c : InName) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) || c == ' ' || c == '-' || c == '_')
                Out.push_back(c);
        }
        // Trim spaces
        while (!Out.empty() && Out.front() == ' ')
            Out.erase(Out.begin());
        while (!Out.empty() && Out.back() == ' ')
            Out.pop_back();
        if (Out.empty() || Out.size() > 64)
            return {};
        // Reserved
        std::string Lower = Out;
        std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (Lower == "default" || Lower == "main")
            return {};
        return Out;
    }

    std::string FEditorLayoutStore::GetLayoutIniPath(const std::string& InName) const {
        return (fs::path(LayoutsDir) / (InName + ".ini")).string();
    }

    std::string FEditorLayoutStore::GetLayoutMetaPath(const std::string& InName) const {
        return (fs::path(LayoutsDir) / (InName + ".panels.json")).string();
    }

    bool FEditorLayoutStore::WritePanelMeta(const std::string& InPath, const std::string& InName,
                                            const FEditorPanelVisibility& InPanels) {
        std::ofstream Out(InPath);
        if (!Out.is_open())
            return false;
        Out << "{\n";
        Out << "  \"Name\": \"" << EscapeJson(InName) << "\",\n";
        Out << "  \"Panels\": {\n";
        Out << "    \"Viewport\": " << (InPanels.bShowViewport ? "true" : "false") << ",\n";
        Out << "    \"PlaceActors\": " << (InPanels.bShowPlaceActors ? "true" : "false") << ",\n";
        Out << "    \"Outliner\": " << (InPanels.bShowOutliner ? "true" : "false") << ",\n";
        Out << "    \"Details\": " << (InPanels.bShowDetails ? "true" : "false") << ",\n";
        Out << "    \"ContentBrowser\": " << (InPanels.bShowContentBrowser ? "true" : "false") << ",\n";
        Out << "    \"OutputLog\": " << (InPanels.bShowOutputLog ? "true" : "false") << ",\n";
        Out << "    \"WorldSettings\": " << (InPanels.bShowWorldSettings ? "true" : "false") << ",\n";
        Out << "    \"ProjectSettings\": " << (InPanels.bShowProjectSettings ? "true" : "false") << "\n";
        Out << "  }\n";
        Out << "}\n";
        return true;
    }

    bool FEditorLayoutStore::ReadPanelMeta(const std::string& InPath, FEditorPanelVisibility& OutPanels) {
        std::ifstream In(InPath);
        if (!In.is_open())
            return false;
        const std::string Content((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
        ParseBoolAfterKey(Content, "Viewport", OutPanels.bShowViewport);
        ParseBoolAfterKey(Content, "PlaceActors", OutPanels.bShowPlaceActors);
        ParseBoolAfterKey(Content, "Outliner", OutPanels.bShowOutliner);
        ParseBoolAfterKey(Content, "Details", OutPanels.bShowDetails);
        ParseBoolAfterKey(Content, "ContentBrowser", OutPanels.bShowContentBrowser);
        ParseBoolAfterKey(Content, "OutputLog", OutPanels.bShowOutputLog);
        ParseBoolAfterKey(Content, "WorldSettings", OutPanels.bShowWorldSettings);
        ParseBoolAfterKey(Content, "ProjectSettings", OutPanels.bShowProjectSettings);
        return true;
    }

    bool FEditorLayoutStore::SaveNamedLayout(const std::string& InName, const FEditorPanelVisibility& InPanels) {
        const std::string Name = SanitizeLayoutName(InName);
        if (Name.empty())
            return false;

        std::error_code Ec;
        fs::create_directories(LayoutsDir, Ec);

        const std::string IniPath = GetLayoutIniPath(Name);
        const std::string MetaPath = GetLayoutMetaPath(Name);

        ImGui::SaveIniSettingsToDisk(IniPath.c_str());
        if (!fs::exists(IniPath, Ec)) {
            LE_CORE_ERROR("FEditorLayoutStore: failed to write layout ini '{}'", IniPath);
            return false;
        }
        if (!WritePanelMeta(MetaPath, Name, InPanels)) {
            LE_CORE_ERROR("FEditorLayoutStore: failed to write layout meta '{}'", MetaPath);
            return false;
        }

        SetActiveLayoutName(Name);
        RefreshLayoutList();
        return true;
    }

    bool FEditorLayoutStore::LoadNamedLayout(const std::string& InName, FEditorPanelVisibility& OutPanels) {
        const std::string Name = SanitizeLayoutName(InName);
        if (Name.empty())
            return false;

        const std::string IniPath = GetLayoutIniPath(Name);
        std::error_code Ec;
        if (!fs::exists(IniPath, Ec)) {
            LE_CORE_WARN("FEditorLayoutStore: layout '{}' not found", Name);
            return false;
        }

        OutPanels = FEditorPanelVisibility{};
        const std::string MetaPath = GetLayoutMetaPath(Name);
        if (fs::exists(MetaPath, Ec))
            ReadPanelMeta(MetaPath, OutPanels);

        ImGui::LoadIniSettingsFromDisk(IniPath.c_str());
        SetActiveLayoutName(Name);
        return true;
    }

    bool FEditorLayoutStore::DeleteNamedLayout(const std::string& InName) {
        const std::string Name = SanitizeLayoutName(InName);
        if (Name.empty())
            return false;

        std::error_code Ec;
        fs::remove(GetLayoutIniPath(Name), Ec);
        fs::remove(GetLayoutMetaPath(Name), Ec);

        if (ActiveLayoutName == Name)
            ClearActiveLayout();

        RefreshLayoutList();
        return true;
    }

    void FEditorLayoutStore::LoadActivePreference() {
        ActiveLayoutName.clear();
        const fs::path PrefPath = fs::path(LayoutsDir) / "ActiveLayout.txt";
        std::error_code Ec;
        if (!fs::exists(PrefPath, Ec))
            return;
        std::ifstream In(PrefPath);
        if (!In.is_open())
            return;
        std::string Line;
        std::getline(In, Line);
        ActiveLayoutName = SanitizeLayoutName(Line);
    }

    void FEditorLayoutStore::SaveActivePreference() const {
        std::error_code Ec;
        fs::create_directories(LayoutsDir, Ec);
        const fs::path PrefPath = fs::path(LayoutsDir) / "ActiveLayout.txt";
        std::ofstream Out(PrefPath);
        if (!Out.is_open())
            return;
        Out << ActiveLayoutName << "\n";
    }

} // namespace Leon::Editor
