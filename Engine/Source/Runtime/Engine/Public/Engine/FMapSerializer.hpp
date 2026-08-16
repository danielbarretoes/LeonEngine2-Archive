#pragma once

#include "Core/Base.hpp"
#include "Engine/UWorld.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Unreal Engine aligned FMapSerializer for saving and loading .lmap files into UWorld.
     */
    class FMapSerializer {
    public:
        explicit FMapSerializer(const TRef<UWorld>& InWorld);

        bool Serialize(const std::string& InFilePath);
        bool SerializeText(std::string& OutYamlString);

        bool Deserialize(const std::string& InFilePath);
        bool DeserializeText(const std::string& InYamlString);

    private:
        TRef<UWorld> World;
    };

} // namespace Leon
