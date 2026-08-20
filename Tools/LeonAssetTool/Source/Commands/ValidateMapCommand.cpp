#include "LeonAssetTool/Commands/ValidateMapCommand.hpp"
#include "Assets/UAssetManager.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Leon::Tools {

    void FValidateMapCommand::PrintHelp() const {
        std::cout << "Usage: " << GetUsage() << "\n\n";
        std::cout << "Options:\n";
        std::cout << "  --map <path.lmap>  Path to level file to validate\n";
    }

    int FValidateMapCommand::Execute(const FCommandArgs& InArgs) {
        std::string mapPath = InArgs.GetOption("map");
        if (mapPath.empty() && !InArgs.PositionalArgs.empty()) {
            mapPath = InArgs.PositionalArgs[0];
        }

        if (mapPath.empty()) {
            std::cerr << "[ERROR] --map <path> is required for validate_map.\n\n";
            PrintHelp();
            return 1;
        }

        if (!fs::exists(mapPath)) {
            std::cerr << "[ERROR] Map file not found: " << mapPath << "\n";
            return 1;
        }

        std::cout << "===============================================================\n";
        std::cout << " LeonEngine2 Map Validator\n";
        std::cout << " Map:   " << mapPath << "\n";
        std::cout << "===============================================================\n\n";

        std::ifstream file(mapPath);
        if (!file.is_open()) {
            std::cerr << "[FAIL] Failed to open map file: " << mapPath << "\n";
            return 1;
        }

        std::string line;
        size_t actorCount = 0;
        size_t staticMeshCount = 0;
        size_t dirLightCount = 0;
        size_t pointLightCount = 0;
        size_t spotLightCount = 0;
        size_t cameraCount = 0;
        size_t missingAssets = 0;

        while (std::getline(file, line)) {
            std::string trimmed = line;
            size_t first = trimmed.find_first_not_of(" \t");
            if (first == std::string::npos)
                continue;
            trimmed = trimmed.substr(first);

            if (trimmed.rfind("- Name:", 0) == 0) {
                actorCount++;
            } else if (trimmed.rfind("StaticMesh:", 0) == 0) {
                staticMeshCount++;
            } else if (trimmed.rfind("DirectionalLight:", 0) == 0) {
                dirLightCount++;
            } else if (trimmed.rfind("PointLight:", 0) == 0) {
                pointLightCount++;
            } else if (trimmed.rfind("SpotLight:", 0) == 0) {
                spotLightCount++;
            } else if (trimmed.rfind("Camera:", 0) == 0) {
                cameraCount++;
            } else if (trimmed.rfind("Asset:", 0) == 0 || trimmed.rfind("HDREnvironmentMap:", 0) == 0) {
                size_t colon = trimmed.find(':');
                std::string assetPath = trimmed.substr(colon + 1);
                size_t q1 = assetPath.find('"');
                size_t q2 = assetPath.rfind('"');
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    assetPath = assetPath.substr(q1 + 1, q2 - q1 - 1);
                } else {
                    size_t p = assetPath.find_first_not_of(" \t\r\n");
                    if (p != std::string::npos)
                        assetPath = assetPath.substr(p);
                }

                if (!assetPath.empty()) {
                    std::string resolved = UAssetManager::ResolveVirtualPath(assetPath);
                    if (!fs::exists(resolved) && !fs::exists(assetPath)) {
                        std::cerr << "  [WARNING] Referenced asset not found on disk: " << assetPath << "\n";
                        missingAssets++;
                    } else {
                        std::cout << "  [PASS] Referenced asset verified: " << assetPath << "\n";
                    }
                }
            }
        }

        std::cout << "\n--- Map Statistics ---\n";
        std::cout << "  Actors:             " << actorCount << "\n";
        std::cout << "  Static Mesh Actors: " << staticMeshCount << "\n";
        std::cout << "  Directional Lights: " << dirLightCount << "\n";
        std::cout << "  Point Lights:       " << pointLightCount << "\n";
        std::cout << "  Spot Lights:        " << spotLightCount << "\n";
        std::cout << "  Cameras:            " << cameraCount << "\n\n";

        if (missingAssets > 0) {
            std::cerr << "[FAIL] Map validation completed with " << missingAssets << " missing asset references.\n";
            return 1;
        }

        std::cout << "===============================================================\n";
        std::cout << " Status: PASSED (Map validated successfully with 0 missing assets)\n";
        std::cout << "===============================================================\n";
        return 0;
    }

} // namespace Leon::Tools
