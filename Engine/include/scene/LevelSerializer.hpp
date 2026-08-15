#pragma once

#include "core/Base.hpp"
#include "scene/Scene.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Serializer and deserializer for LeonEngine2 level asset files (.llevel).
     * Parses and generates declarative level files containing environment settings,
     * actors, components, meshes, PBR materials, lights, and cameras.
     */
    class FLevelSerializer {
    public:
        explicit FLevelSerializer(const TRef<FScene>& InScene);

        bool Serialize(const std::string& InFilePath);
        bool Deserialize(const std::string& InFilePath);

        bool SerializeText(std::string& OutText);
        bool DeserializeText(const std::string& InText);

    private:
        TRef<FScene> m_Scene;
    };

} // namespace Leon
