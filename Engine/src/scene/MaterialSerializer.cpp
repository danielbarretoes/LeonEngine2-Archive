#include "scene/MaterialSerializer.hpp"
#include "core/Log.hpp"
#include "renderer/AssetManager.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace Leon {

    namespace MaterialUtils {

        static std::string Trim(const std::string& InStr) {
            size_t first = InStr.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return "";
            size_t last = InStr.find_last_not_of(" \t\r\n");
            return InStr.substr(first, (last - first + 1));
        }

        static std::string CleanValue(const std::string& InStr) {
            std::string s = Trim(InStr);
            if (s.length() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
                s = s.substr(1, s.length() - 2);
            }
            return s;
        }

        static float ParseFloat(const std::string& InStr, float InDefault = 0.0f) {
            std::string s = CleanValue(InStr);
            if (s.empty())
                return InDefault;
            try {
                return std::stof(s);
            } catch (...) {
                return InDefault;
            }
        }

        static bool ParseBool(const std::string& InStr, bool InDefault = false) {
            std::string s = CleanValue(InStr);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (s == "true" || s == "1" || s == "yes" || s == "on")
                return true;
            if (s == "false" || s == "0" || s == "no" || s == "off")
                return false;
            return InDefault;
        }

        static glm::vec3 ParseVec3(const std::string& InStr, const glm::vec3& InDefault = glm::vec3(1.0f)) {
            std::string s = CleanValue(InStr);
            if (s.front() == '[')
                s = s.substr(1);
            if (!s.empty() && s.back() == ']')
                s.pop_back();

            std::stringstream ss(s);
            std::string item;
            std::vector<float> values;
            while (std::getline(ss, item, ',')) {
                values.push_back(ParseFloat(item));
            }
            if (values.size() >= 3)
                return glm::vec3(values[0], values[1], values[2]);
            if (values.size() == 1)
                return glm::vec3(values[0]);
            return InDefault;
        }

    } // namespace MaterialUtils

    bool FMaterialSerializer::Serialize(const std::string& InFilePath, const FPBRMaterial& InMaterial) {
        std::string text;
        if (!SerializeText(text, InMaterial))
            return false;

        std::ofstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FMaterialSerializer: Could not open '{0}' for writing!", InFilePath);
            return false;
        }

        file << text;
        LE_CORE_INFO("FMaterialSerializer: Saved material to '{0}'", InFilePath);
        return true;
    }

    bool FMaterialSerializer::SerializeText(std::string& OutText, const FPBRMaterial& InMaterial) {
        std::stringstream ss;
        ss << "# LeonEngine2 Material Asset File (.lmat)\n";
        ss << "Material:\n";
        ss << "  Type: \"PBR_Lit\"\n";
        ss << "  AlbedoColor: [" << InMaterial.AlbedoColor.r << ", " << InMaterial.AlbedoColor.g << ", "
           << InMaterial.AlbedoColor.b << "]\n";
        ss << "  Metallic: " << InMaterial.Metallic << "\n";
        ss << "  Roughness: " << InMaterial.Roughness << "\n";
        ss << "  AO: " << InMaterial.AO << "\n";

        if (InMaterial.AlbedoMap)
            ss << "  AlbedoMap: \"" << InMaterial.AlbedoMap->GetPath() << "\"\n";
        if (InMaterial.NormalMap)
            ss << "  NormalMap: \"" << InMaterial.NormalMap->GetPath() << "\"\n";
        if (InMaterial.MetallicMap)
            ss << "  MetallicMap: \"" << InMaterial.MetallicMap->GetPath() << "\"\n";
        if (InMaterial.RoughnessMap)
            ss << "  RoughnessMap: \"" << InMaterial.RoughnessMap->GetPath() << "\"\n";
        if (InMaterial.AOMap)
            ss << "  AOMap: \"" << InMaterial.AOMap->GetPath() << "\"\n";

        ss << "  UsePlanarReflection: " << (InMaterial.bUsePlanarReflection ? "true" : "false") << "\n";

        OutText = ss.str();
        return true;
    }

    bool FMaterialSerializer::Deserialize(const std::string& InFilePath, FPBRMaterial& OutMaterial) {
        std::ifstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FMaterialSerializer: Could not open material file '{0}' for loading!", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        return DeserializeText(ss.str(), OutMaterial);
    }

    bool FMaterialSerializer::DeserializeText(const std::string& InText, FPBRMaterial& OutMaterial) {
        std::stringstream ss(InText);
        std::string line;

        while (std::getline(ss, line)) {
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            std::string trimmed = MaterialUtils::Trim(line);
            if (trimmed.empty())
                continue;

            size_t colonPos = trimmed.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = MaterialUtils::Trim(trimmed.substr(0, colonPos));
            std::string value = MaterialUtils::Trim(trimmed.substr(colonPos + 1));

            if (value.empty())
                continue;

            if (key == "AlbedoColor")
                OutMaterial.AlbedoColor = MaterialUtils::ParseVec3(value, OutMaterial.AlbedoColor);
            else if (key == "Metallic")
                OutMaterial.Metallic = MaterialUtils::ParseFloat(value, 0.0f);
            else if (key == "Roughness")
                OutMaterial.Roughness = MaterialUtils::ParseFloat(value, 0.5f);
            else if (key == "AO")
                OutMaterial.AO = MaterialUtils::ParseFloat(value, 1.0f);
            else if (key == "AlbedoMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.AlbedoMap = FAssetManager::GetTexture2D(path);
                OutMaterial.bUseAlbedoMap = (OutMaterial.AlbedoMap != nullptr);
            } else if (key == "NormalMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.NormalMap = FAssetManager::GetTexture2D(path);
                OutMaterial.bUseNormalMap = (OutMaterial.NormalMap != nullptr);
            } else if (key == "MetallicMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.MetallicMap = FAssetManager::GetTexture2D(path);
                OutMaterial.bUseMetallicMap = (OutMaterial.MetallicMap != nullptr);
            } else if (key == "RoughnessMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.RoughnessMap = FAssetManager::GetTexture2D(path);
                OutMaterial.bUseRoughnessMap = (OutMaterial.RoughnessMap != nullptr);
            } else if (key == "AOMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.AOMap = FAssetManager::GetTexture2D(path);
                OutMaterial.bUseAOMap = (OutMaterial.AOMap != nullptr);
            } else if (key == "UsePlanarReflection") {
                OutMaterial.bUsePlanarReflection = MaterialUtils::ParseBool(value, false);
            }
        }

        return true;
    }

} // namespace Leon
