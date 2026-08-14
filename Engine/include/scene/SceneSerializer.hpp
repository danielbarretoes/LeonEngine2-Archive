#pragma once

#include "core/Base.hpp"
#include "scene/Scene.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Serializer and deserializer for LeonEngine2 level files (.llevel, .umap).
     * Parses and generates Unreal Engine-inspired structured scene files containing
     * environment settings, actors, components, meshes, PBR materials, lights, and text.
     */
    class FSceneSerializer {
    public:
        explicit FSceneSerializer(const TRef<FScene>& InScene);

        bool Serialize(const std::string& InFilePath);
        bool Deserialize(const std::string& InFilePath);

        bool SerializeText(std::string& OutText);
        bool DeserializeText(const std::string& InText);

    private:
        TRef<FScene> m_Scene;
    };

} // namespace Leon
