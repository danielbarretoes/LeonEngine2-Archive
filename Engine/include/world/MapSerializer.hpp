#pragma once

#include "core/Base.hpp"
#include "world/UWorld.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Unreal Engine aligned MapSerializer for saving and loading .lmap files into UWorld.
     */
    class MapSerializer {
    public:
        explicit MapSerializer(const TRef<UWorld>& InWorld);

        bool Serialize(const std::string& InFilePath);
        bool SerializeText(std::string& OutYamlString);

        bool Deserialize(const std::string& InFilePath);
        bool DeserializeText(const std::string& InYamlString);

    private:
        TRef<UWorld> m_World;
    };

} // namespace Leon
