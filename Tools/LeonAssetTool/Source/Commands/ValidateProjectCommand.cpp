#include "LeonAssetTool/Commands/ValidateProjectCommand.hpp"
#include "LeonAssetTool/Commands/ValidateMapCommand.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Assets/FAssetPath.hpp"

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FValidateProjectCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --project <path.lproject>  Path to project descriptor to validate\n";
    }

    int FValidateProjectCommand::Execute(const FCommandArgs& InArgs) {
        std::string projectPath = InArgs.GetOption("project");
        if (projectPath.empty() && !InArgs.PositionalArgs.empty()) {
            projectPath = InArgs.PositionalArgs[0];
        }

        if (projectPath.empty()) {
            std::cerr << "[ERROR] --project <path.lproject> is required for validate_project.\n\n";
            PrintHelp();
            return 1;
        }

        std::cout << "===============================================================\n";
        std::cout << " LeonEngine2 Project Validator\n";
        std::cout << " Project File: " << projectPath << "\n";
        std::cout << "===============================================================\n\n";

        fs::path projPath = projectPath;
        if (!fs::exists(projPath)) {
            std::cerr << "  [FAIL] Project file not found: " << projectPath << "\n";
            return 1;
        }

        // 1. Load Descriptor
        FProjectDescriptor desc;
        if (!desc.Load(projectPath)) {
            std::cerr << "  [FAIL] Failed to parse .lproject JSON descriptor.\n";
            return 1;
        }

        std::cout << "  [PASS] Project Descriptor loaded successfully:\n";
        std::cout << "         Project Name:     " << desc.ProjectName << "\n";
        std::cout << "         Engine Version:   " << desc.EngineVersion << "\n";
        std::cout << "         Default Map:      " << desc.DefaultMap << "\n";
        std::cout << "         Default GameMode: " << desc.DefaultGameMode << "\n\n";

        FProjectPaths::SetProjectRoot(projectPath);
        std::string projDir = FProjectPaths::ProjectDir();
        std::string configDir = FProjectPaths::ProjectConfigDir();
        std::string contentDir = FProjectPaths::ProjectContentDir();

        // 2. Validate Project Directories
        if (!fs::exists(configDir)) {
            std::cerr << "  [FAIL] Config directory missing: " << configDir << "\n";
            return 1;
        }
        std::cout << "  [PASS] Config directory verified: " << configDir << "\n";

        if (!fs::exists(contentDir)) {
            std::cerr << "  [FAIL] Content directory missing: " << contentDir << "\n";
            return 1;
        }
        std::cout << "  [PASS] Content directory verified: " << contentDir << "\n";

        // 3. Validate Configuration Files
        std::string engineIni = FAssetPath::Combine(configDir, "DefaultEngine.ini");
        if (fs::exists(engineIni)) {
            std::cout << "  [PASS] Configuration file verified: " << engineIni << "\n";
        } else {
            std::cerr << "  [WARN] DefaultEngine.ini missing in config directory.\n";
        }

        std::string gameIni = FAssetPath::Combine(configDir, "DefaultGame.ini");
        if (fs::exists(gameIni)) {
            std::cout << "  [PASS] Configuration file verified: " << gameIni << "\n";
        }

        std::string inputIni = FAssetPath::Combine(configDir, "DefaultInput.ini");
        if (fs::exists(inputIni)) {
            std::cout << "  [PASS] Configuration file verified: " << inputIni << "\n";
        }

        // 4. Resolve Default Map
        std::string physicalMap = FProjectPaths::ResolveVirtualPath(desc.DefaultMap);
        if (!fs::exists(physicalMap)) {
            std::cerr << "  [FAIL] Default map '" << desc.DefaultMap << "' resolved to '" << physicalMap
                      << "' which does NOT exist on disk.\n";
            return 1;
        }
        std::cout << "  [PASS] Virtual path resolved: " << desc.DefaultMap << " -> " << physicalMap << "\n\n";

        // 5. Validate Default Map Assets
        std::cout << "--- Validating Default Map Contents ---\n";
        FValidateMapCommand mapCmd;
        FCommandArgs mapArgs;
        mapArgs.Options["map"] = physicalMap;
        int mapResult = mapCmd.Execute(mapArgs);
        if (mapResult != 0) {
            std::cerr << "\n===============================================================\n";
            std::cerr << " Status: FAILED (Project has map asset integrity issues)\n";
            std::cerr << "===============================================================\n";
            return 1;
        }

        std::cout << "\n===============================================================\n";
        std::cout << " Status: PASSED (Project structure and assets verified successfully)\n";
        std::cout << "===============================================================\n";
        return 0;
    }

} // namespace Leon::Tools
