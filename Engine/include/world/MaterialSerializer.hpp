#pragma once

#include "core/Base.hpp"
#include "renderer/Material.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Serializer and deserializer for LeonEngine2 Material Asset Files (.lmat).
     */
    class FMaterialSerializer {
    public:
        static bool Serialize(const std::string& InFilePath, const FMaterial& InMaterial);
        static bool Deserialize(const std::string& InFilePath, FMaterial& OutMaterial);

        static bool SerializeText(std::string& OutText, const FMaterial& InMaterial);
        static bool DeserializeText(const std::string& InText, FMaterial& OutMaterial);
    };

} // namespace Leon
