#include "core/ProjectPaths.hpp"
#include "asset/AssetPath.hpp"
#include <filesystem>
#include <algorithm>

namespace Leon {

    std::string FProjectPaths::s_ProjectDir = "Projects/Sandbox";

    void FProjectPaths::SetProjectRoot(const std::string& InProjectFilePathOrDir) {
        std::string norm = FAssetPath::Normalize(InProjectFilePathOrDir);
        if (norm.empty()) {
            s_ProjectDir = "Projects/Sandbox";
            return;
        }

        // If a file was passed (e.g. Projects/Sandbox/Sandbox.lproject), take parent directory
        if (FAssetPath::GetExtension(norm) == "lproject" || FAssetPath::GetExtension(norm) == "ini") {
            s_ProjectDir = FAssetPath::GetDirectory(norm);
        } else {
            s_ProjectDir = norm;
        }
        if (s_ProjectDir.empty()) {
            s_ProjectDir = ".";
        }
    }

    const std::string& FProjectPaths::ProjectDir() {
        return s_ProjectDir;
    }

    std::string FProjectPaths::ProjectContentDir() {
        return FAssetPath::Combine(s_ProjectDir, "Content");
    }

    std::string FProjectPaths::ProjectConfigDir() {
        return FAssetPath::Combine(s_ProjectDir, "Config");
    }

    std::string FProjectPaths::ProjectSourceDir() {
        return FAssetPath::Combine(s_ProjectDir, "Source");
    }

    std::string FProjectPaths::ProjectSavedDir() {
        return FAssetPath::Combine(s_ProjectDir, "Saved");
    }

    std::string FProjectPaths::ProjectIntermediateDir() {
        return FAssetPath::Combine(s_ProjectDir, "Intermediate");
    }

    std::string FProjectPaths::EngineDir() {
        return "Engine";
    }

    std::string FProjectPaths::EngineContentDir() {
        return "Engine/Assets";
    }

    std::string FProjectPaths::EngineConfigDir() {
        return "Engine/Config";
    }

    std::string FProjectPaths::ResolveVirtualPath(const std::string& InVirtualPath) {
        if (InVirtualPath.empty()) {
            return "";
        }

        std::string norm = FAssetPath::Normalize(InVirtualPath);

        // 1. Virtual Path: /Game/...
        if (norm.rfind("/Game/", 0) == 0 || norm.rfind("Game/", 0) == 0) {
            std::string subPath = (norm.rfind("/Game/", 0) == 0) ? norm.substr(6) : norm.substr(5);
            std::string contentDir = ProjectContentDir();
            std::string directCombined = FAssetPath::Combine(contentDir, subPath);

            // If direct file exists with specified extension
            if (std::filesystem::exists(directCombined)) {
                return directCombined;
            }

            // Auto-deduce extension if not present
            if (FAssetPath::GetExtension(subPath).empty()) {
                const std::vector<std::string> candidateExts = {
                    ".lmap", ".lmat", ".lmesh", ".ltex", ".lhdr", ".lmi", ".png", ".glsl"
                };
                for (const auto& ext : candidateExts) {
                    std::string testPath = directCombined + ext;
                    if (std::filesystem::exists(testPath)) {
                        return testPath;
                    }
                }
            }

            return directCombined;
        }

        // 2. Virtual Path: /Engine/...
        if (norm.rfind("/Engine/", 0) == 0 || norm.rfind("Engine/", 0) == 0) {
            std::string subPath = (norm.rfind("/Engine/", 0) == 0) ? norm.substr(8) : norm.substr(7);
            std::string engineAssetsDir = EngineContentDir();
            std::string directCombined = FAssetPath::Combine(engineAssetsDir, subPath);

            if (std::filesystem::exists(directCombined)) {
                return directCombined;
            }

            // Also check directly under Engine/
            std::string engineDirect = FAssetPath::Combine(EngineDir(), subPath);
            if (std::filesystem::exists(engineDirect)) {
                return engineDirect;
            }

            if (FAssetPath::GetExtension(subPath).empty()) {
                const std::vector<std::string> candidateExts = {
                    ".glsl", ".ttf", ".bin", ".ltex", ".lmesh", ".lmat"
                };
                for (const auto& ext : candidateExts) {
                    std::string testPath = directCombined + ext;
                    if (std::filesystem::exists(testPath)) {
                        return testPath;
                    }
                }
            }

            return directCombined;
        }

        // 3. Direct file check on disk
        if (std::filesystem::exists(norm)) {
            return norm;
        }

        // 4. Relative to Project Content directory
        std::string inContent = FAssetPath::Combine(ProjectContentDir(), norm);
        if (std::filesystem::exists(inContent)) {
            return inContent;
        }

        // 5. Relative to Engine Content directory
        std::string inEngine = FAssetPath::Combine(EngineContentDir(), norm);
        if (std::filesystem::exists(inEngine)) {
            return inEngine;
        }

        return norm;
    }

    std::string FProjectPaths::MakeVirtualPath(const std::string& InPhysicalPath) {
        std::string norm = FAssetPath::Normalize(InPhysicalPath);
        std::string contentDir = FAssetPath::Normalize(ProjectContentDir());
        std::string engineContent = FAssetPath::Normalize(EngineContentDir());

        if (norm.rfind(contentDir, 0) == 0) {
            std::string rel = norm.substr(contentDir.length());
            if (!rel.empty() && rel.front() == '/') rel = rel.substr(1);
            return "/Game/" + rel;
        }

        if (norm.rfind(engineContent, 0) == 0) {
            std::string rel = norm.substr(engineContent.length());
            if (!rel.empty() && rel.front() == '/') rel = rel.substr(1);
            return "/Engine/" + rel;
        }

        return norm;
    }

} // namespace Leon
