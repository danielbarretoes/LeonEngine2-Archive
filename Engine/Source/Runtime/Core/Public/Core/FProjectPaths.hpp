#pragma once

#include "Core/Base.hpp"
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Unreal Engine-aligned Project Paths and Virtual Path Resolver (/Game/..., /Engine/...).
     */
    class FProjectPaths {
    public:
        static void SetProjectRoot(const std::string& InProjectFilePathOrDir);
        static const std::string& ProjectDir();
        static std::string ProjectContentDir();
        static std::string ProjectConfigDir();
        static std::string ProjectSourceDir();
        static std::string ProjectSavedDir();
        static std::string ProjectIntermediateDir();

        static std::string EngineDir();
        static std::string EngineContentDir();
        static std::string EngineConfigDir();

        /** Resolve virtual path (/Game/..., /Engine/...) to physical disk path */
        static std::string ResolveVirtualPath(const std::string& InVirtualPath);

        /** Convert physical disk path to virtual package path (/Game/..., /Engine/...) */
        static std::string MakeVirtualPath(const std::string& InPhysicalPath);

        /**
         * @brief Locate a .lproject file from a path hint.
         * Tries the path as-is, directory scan, then walks parent dirs for the relative
         * hint or any .lproject beside an Engine/ folder.
         */
        static std::string LocateProjectFile(const std::string& InPathOrDir);

        /**
         * @brief If argv0 sits next to Engine/Assets (shipping layout), chdir there.
         * Dev builds keep the caller's cwd (engine root) because that folder is absent.
         */
        static bool AdoptPackagedWorkingDirectory(const char* InExecutableArgv0);

    private:
        static std::string CachedProjectDir;
    };

} // namespace Leon
