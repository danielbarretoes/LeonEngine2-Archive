#pragma once

#include "core/Base.hpp"
#include <string>

namespace Leon {

    /**
     * @brief Project descriptor file (.lproject) format.
     * Equivalent to Unreal Engine's .uproject JSON descriptor.
     */
    struct FProjectDescriptor {
        uint32_t FileVersion = 1;
        std::string EngineVersion = "0.8.0";
        std::string ProjectName = "Sandbox";
        std::string DefaultMap = "/Game/Maps/MainShowcase";
        std::string DefaultGameMode = "AGameModeBase";

        bool Load(const std::string& InFilePath);
        bool Save(const std::string& InFilePath) const;
        bool DeserializeJson(const std::string& InJsonString);
        std::string SerializeJson() const;
    };

} // namespace Leon
