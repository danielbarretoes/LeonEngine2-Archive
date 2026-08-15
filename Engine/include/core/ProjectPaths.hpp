#pragma once

#include "core/Base.hpp"
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

    private:
        static std::string s_ProjectDir;
    };

} // namespace Leon
