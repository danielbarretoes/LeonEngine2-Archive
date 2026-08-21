#pragma once

#include "Core/Base.hpp"
#include <string>

namespace Leon {

    /**
     * @brief Project descriptor file (.lproject) format.
     * Equivalent to Unreal Engine's .uproject JSON descriptor.
     */
    struct FProjectDescriptor {
        uint32_t FileVersion = 1;
        std::string EngineVersion = "0.15.0";
        std::string ProjectName = "Project";
        std::string DefaultMap = "/Game/Maps/Untitled";
        std::string DefaultGameMode = "AGameModeBase";
        /** Optional path to game module DLL (relative to project dir or absolute). */
        std::string GameModule;

        bool Load(const std::string& InFilePath);
        bool Save(const std::string& InFilePath) const;
        bool DeserializeJson(const std::string& InJsonString);
        std::string SerializeJson() const;
    };

} // namespace Leon
