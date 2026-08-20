#include "LeonAssetTool/Commands/BakeLightmapsCommand.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Assets/UAssetManager.hpp"
#include "Lightmass/FLightmass.hpp"

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FBakeLightmapsCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --map <path.lmap>                 Path to map level file to bake\n";
        std::cout << "  --quality=Preview|Draft|Production Lighting build quality (default: Draft)\n";
        std::cout << "  --force                           Force re-baking lightmaps ignoring cache\n";
    }

    int FBakeLightmapsCommand::Execute(const FCommandArgs& InArgs) {
        std::string mapPath = InArgs.GetOption("map");
        if (mapPath.empty() && !InArgs.PositionalArgs.empty()) {
            mapPath = InArgs.PositionalArgs[0];
        }
        std::string qualityStr = InArgs.GetOption("quality", "Draft");
        bool bForce = InArgs.HasFlag("force");

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

        FLightmassSettings settings;
        ELightingBuildQuality quality = ELightingBuildQuality::Draft;
        if (qualityStr == "Preview")
            quality = ELightingBuildQuality::Preview;
        else if (qualityStr == "Production")
            quality = ELightingBuildQuality::Production;
        ApplyLightingBuildQuality(quality, settings);

        auto result = FLightmass::BakeMap(mapPath, settings, bForce);
        UAssetManager::Shutdown();
        return result.bSuccess ? 0 : 1;
    }

} // namespace Leon::Tools
