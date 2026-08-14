#pragma once

#include "core/Base.hpp"
#include "scene/Components.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Serializer and deserializer for LeonEngine2 Material Asset Files (.lmat).
     */
    class FMaterialSerializer {
    public:
        static bool Serialize(const std::string& InFilePath, const FPBRMaterial& InMaterial);
        static bool Deserialize(const std::string& InFilePath, FPBRMaterial& OutMaterial);

        static bool SerializeText(std::string& OutText, const FPBRMaterial& InMaterial);
        static bool DeserializeText(const std::string& InText, FPBRMaterial& OutMaterial);
    };

    using MaterialSerializer = FMaterialSerializer;

} // namespace Leon
