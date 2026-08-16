#include "Core/FProjectPaths.hpp"
#include "Assets/FAssetPath.hpp"
#include <filesystem>
#include <algorithm>

namespace Leon {

    std::string FProjectPaths::CachedProjectDir = ".";

    void FProjectPaths::SetProjectRoot(const std::string& InProjectFilePathOrDir) {
        std::string norm = FAssetPath::Normalize(InProjectFilePathOrDir);
        if (norm.empty()) {
            CachedProjectDir = ".";
            return;
        }

        // File paths (e.g. MyGame.lproject) resolve to the parent directory
        if (FAssetPath::GetExtension(norm) == "lproject" || FAssetPath::GetExtension(norm) == "ini") {
            CachedProjectDir = FAssetPath::GetDirectory(norm);
        } else {
            CachedProjectDir = norm;
        }
        if (CachedProjectDir.empty()) {
            CachedProjectDir = ".";
        }
    }

    const std::string& FProjectPaths::ProjectDir() {
        return CachedProjectDir;
    }

    std::string FProjectPaths::ProjectContentDir() {
        return FAssetPath::Combine(CachedProjectDir, "Content");
    }

    std::string FProjectPaths::ProjectConfigDir() {
        return FAssetPath::Combine(CachedProjectDir, "Config");
    }

    std::string FProjectPaths::ProjectSourceDir() {
        return FAssetPath::Combine(CachedProjectDir, "Source");
    }

    std::string FProjectPaths::ProjectSavedDir() {
        return FAssetPath::Combine(CachedProjectDir, "Saved");
    }

    std::string FProjectPaths::ProjectIntermediateDir() {
        return FAssetPath::Combine(CachedProjectDir, "Intermediate");
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

        if (norm.rfind("/Game/", 0) == 0 || norm.rfind("Game/", 0) == 0) {
            std::string subPath = (norm.rfind("/Game/", 0) == 0) ? norm.substr(6) : norm.substr(5);
            std::string contentDir = ProjectContentDir();
            std::string directCombined = FAssetPath::Combine(contentDir, subPath);

            if (std::filesystem::exists(directCombined)) {
                return directCombined;
            }

            if (FAssetPath::GetExtension(subPath).empty()) {
                const std::vector<std::string> candidateExts = {".lmap", ".lmat", ".lmesh", ".ltex",
                                                                ".lhdr", ".lmi",  ".png",   ".glsl"};
                for (const auto& ext : candidateExts) {
                    std::string testPath = directCombined + ext;
                    if (std::filesystem::exists(testPath)) {
                        return testPath;
                    }
                }
            }

            return directCombined;
        }

        if (norm.rfind("/Engine/", 0) == 0 || norm.rfind("Engine/", 0) == 0) {
            std::string subPath = (norm.rfind("/Engine/", 0) == 0) ? norm.substr(8) : norm.substr(7);
            std::string engineAssetsDir = EngineContentDir();
            std::string directCombined = FAssetPath::Combine(engineAssetsDir, subPath);

            if (std::filesystem::exists(directCombined)) {
                return directCombined;
            }

            std::string engineDirect = FAssetPath::Combine(EngineDir(), subPath);
            if (std::filesystem::exists(engineDirect)) {
                return engineDirect;
            }

            if (FAssetPath::GetExtension(subPath).empty()) {
                const std::vector<std::string> candidateExts = {".glsl", ".ttf", ".bin", ".ltex", ".lmesh", ".lmat"};
                for (const auto& ext : candidateExts) {
                    std::string testPath = directCombined + ext;
                    if (std::filesystem::exists(testPath)) {
                        return testPath;
                    }
                }
            }

            return directCombined;
        }

        if (std::filesystem::exists(norm)) {
            return norm;
        }

        std::string inContent = FAssetPath::Combine(ProjectContentDir(), norm);
        if (std::filesystem::exists(inContent)) {
            return inContent;
        }

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
            if (!rel.empty() && rel.front() == '/')
                rel = rel.substr(1);
            return "/Game/" + rel;
        }

        if (norm.rfind(engineContent, 0) == 0) {
            std::string rel = norm.substr(engineContent.length());
            if (!rel.empty() && rel.front() == '/')
                rel = rel.substr(1);
            return "/Engine/" + rel;
        }

        return norm;
    }

    std::string FProjectPaths::LocateProjectFile(const std::string& InPathOrDir) {
        namespace fs = std::filesystem;

        auto findInDirectory = [](const fs::path& dir) -> std::string {
            if (!fs::is_directory(dir))
                return {};
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.path().extension() == ".lproject") {
                    return FAssetPath::Normalize(entry.path().string());
                }
            }
            return {};
        };

        auto tryPath = [&](const fs::path& candidate) -> std::string {
            if (candidate.empty())
                return {};
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec) && candidate.extension() == ".lproject") {
                return FAssetPath::Normalize(candidate.string());
            }
            if (fs::is_directory(candidate, ec)) {
                return findInDirectory(candidate);
            }
            return {};
        };

        // 1. As-given (cwd-relative or absolute)
        if (!InPathOrDir.empty()) {
            if (auto hit = tryPath(InPathOrDir); !hit.empty())
                return hit;
        }

        // 2. Walk parents: relative hint, then any .lproject beside Engine/ (or under Projects/)
        fs::path cursor = fs::current_path();
        for (int depth = 0; depth < 8; ++depth) {
            if (!InPathOrDir.empty()) {
                if (auto hit = tryPath(cursor / InPathOrDir); !hit.empty())
                    return hit;
            }
            if (fs::is_directory(cursor / "Engine")) {
                // Prefer Projects/*/*.lproject without naming a specific game
                fs::path projectsDir = cursor / "Projects";
                if (fs::is_directory(projectsDir)) {
                    for (const auto& entry : fs::directory_iterator(projectsDir)) {
                        if (!entry.is_directory())
                            continue;
                        if (auto hit = findInDirectory(entry.path()); !hit.empty())
                            return hit;
                    }
                }
                if (auto hit = findInDirectory(cursor); !hit.empty())
                    return hit;
            }
            if (!cursor.has_parent_path() || cursor == cursor.root_path())
                break;
            cursor = cursor.parent_path();
        }

        return {};
    }

} // namespace Leon
