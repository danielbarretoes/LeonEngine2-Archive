#include "Lightmass/Commands/ValidateCommand.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Assets/UAssetManager.hpp"
#include "Lightmass/FLightmass.hpp"

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FValidateCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --map <path.lmap>  Path to level file to validate lightmaps for\n";
    }

    int FValidateCommand::Execute(const FCommandArgs& InArgs) {
        std::string mapPath = InArgs.GetOption("map");
        if (mapPath.empty() && !InArgs.PositionalArgs.empty()) {
            mapPath = InArgs.PositionalArgs[0];
        }

        if (mapPath.empty() || !fs::exists(mapPath)) {
            std::cerr << "[ERROR] --map <path.lmap> is required and must exist.\n\n";
            PrintHelp();
            return 1;
        }

        FLog::Init();
        UAssetManager::Init();

        fs::path p = mapPath;
        fs::path contentRoot = p.parent_path();
        if (contentRoot.filename() == "Maps")
            contentRoot = contentRoot.parent_path();
        UAssetManager::SetContentRoot(contentRoot.string());
        FProjectPaths::SetProjectRoot(contentRoot.parent_path().string());

        std::string message;
        bool ok = FLightmass::ValidateMap(mapPath, message);
        std::cout << "[Lightmass] " << message << "\n";
        UAssetManager::Shutdown();

        return ok ? 0 : 1;
    }

} // namespace Leon::Tools
