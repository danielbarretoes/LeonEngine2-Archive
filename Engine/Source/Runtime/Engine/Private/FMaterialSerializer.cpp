#include "Engine/FMaterialSerializer.hpp"
#include "Core/FLog.hpp"
#include "Core/FStringUtils.hpp"
#include "Assets/UAssetManager.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace Leon {

    namespace MaterialUtils {

        static std::string CleanValue(const std::string& InStr) {
            std::string s = FStringUtils::Trim(InStr);
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

    bool FMaterialSerializer::Serialize(const std::string& InFilePath, const FMaterial& InMaterial) {
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

    bool FMaterialSerializer::SerializeText(std::string& OutText, const FMaterial& InMaterial) {
        std::stringstream ss;
        ss << "# LeonEngine2 Material Asset File (.lmat)\n";
        ss << "Material:\n";
        ss << "  Type: \"PBR_Lit\"\n";
        ss << "  AlbedoColor: [" << InMaterial.GetAlbedoColor().r << ", " << InMaterial.GetAlbedoColor().g << ", "
           << InMaterial.GetAlbedoColor().b << "]\n";
        ss << "  Metallic: " << InMaterial.GetMetallic() << "\n";
        ss << "  Roughness: " << InMaterial.GetRoughness() << "\n";
        ss << "  AO: " << InMaterial.GetAO() << "\n";
        ss << "  NormalScale: " << InMaterial.GetNormalScale() << "\n";
        ss << "  OcclusionStrength: " << InMaterial.GetOcclusionStrength() << "\n";
        if (InMaterial.GetEmissiveIntensity() > 0.0f) {
            ss << "  EmissiveColor: [" << InMaterial.GetEmissiveColor().r << ", " << InMaterial.GetEmissiveColor().g
               << ", " << InMaterial.GetEmissiveColor().b << "]\n";
            ss << "  EmissiveIntensity: " << InMaterial.GetEmissiveIntensity() << "\n";
        }

        const char* alphaModeStr = "Opaque";
        if (InMaterial.GetAlphaMode() == EAlphaMode::Mask)
            alphaModeStr = "Mask";
        else if (InMaterial.GetAlphaMode() == EAlphaMode::Blend)
            alphaModeStr = "Blend";
        ss << "  AlphaMode: \"" << alphaModeStr << "\"\n";
        ss << "  AlphaCutoff: " << InMaterial.GetAlphaCutoff() << "\n";
        ss << "  DoubleSided: " << (InMaterial.GetDoubleSided() ? "true" : "false") << "\n";
        ss << "  UVTiling: [" << InMaterial.GetUVTiling().x << ", " << InMaterial.GetUVTiling().y << "]\n";
        ss << "  UVOffset: [" << InMaterial.GetUVOffset().x << ", " << InMaterial.GetUVOffset().y << "]\n";

        std::string albedoPath =
            InMaterial.GetAlbedoMap() ? InMaterial.GetAlbedoMap()->GetPath() : InMaterial.GetTexturePath(0);
        if (!albedoPath.empty())
            ss << "  AlbedoMap: \"" << albedoPath << "\"\n";

        std::string normalPath =
            InMaterial.GetNormalMap() ? InMaterial.GetNormalMap()->GetPath() : InMaterial.GetTexturePath(1);
        if (!normalPath.empty())
            ss << "  NormalMap: \"" << normalPath << "\"\n";

        std::string metallicPath =
            InMaterial.GetMetallicMap() ? InMaterial.GetMetallicMap()->GetPath() : InMaterial.GetTexturePath(2);
        if (!metallicPath.empty())
            ss << "  MetallicMap: \"" << metallicPath << "\"\n";

        std::string roughnessPath =
            InMaterial.GetRoughnessMap() ? InMaterial.GetRoughnessMap()->GetPath() : InMaterial.GetTexturePath(3);
        if (!roughnessPath.empty())
            ss << "  RoughnessMap: \"" << roughnessPath << "\"\n";

        std::string aoPath = InMaterial.GetAOMap() ? InMaterial.GetAOMap()->GetPath() : InMaterial.GetTexturePath(4);
        if (!aoPath.empty())
            ss << "  AOMap: \"" << aoPath << "\"\n";

        std::string emissivePath =
            InMaterial.GetEmissiveMap() ? InMaterial.GetEmissiveMap()->GetPath() : InMaterial.GetTexturePath(5);
        if (!emissivePath.empty())
            ss << "  EmissiveMap: \"" << emissivePath << "\"\n";

        ss << "  UsePlanarReflection: " << (InMaterial.GetUsePlanarReflection() ? "true" : "false") << "\n";

        OutText = ss.str();
        return true;
    }

    bool FMaterialSerializer::Deserialize(const std::string& InFilePath, FMaterial& OutMaterial) {
        std::ifstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FMaterialSerializer: Could not open material file '{0}' for loading!", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        return DeserializeText(ss.str(), OutMaterial);
    }

    bool FMaterialSerializer::DeserializeText(const std::string& InText, FMaterial& OutMaterial) {
        std::stringstream ss(InText);
        std::string line;

        while (std::getline(ss, line)) {
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            std::string trimmed = FStringUtils::Trim(line);
            if (trimmed.empty())
                continue;

            size_t colonPos = trimmed.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = FStringUtils::Trim(trimmed.substr(0, colonPos));
            std::string value = FStringUtils::Trim(trimmed.substr(colonPos + 1));

            if (value.empty())
                continue;

            if (key == "AlbedoColor" || key == "BaseColor")
                OutMaterial.SetAlbedoColor(MaterialUtils::ParseVec3(value, OutMaterial.GetAlbedoColor()));
            else if (key == "Metallic")
                OutMaterial.SetMetallic(MaterialUtils::ParseFloat(value, 0.0f));
            else if (key == "Roughness")
                OutMaterial.SetRoughness(MaterialUtils::ParseFloat(value, 0.5f));
            else if (key == "AO")
                OutMaterial.SetAO(MaterialUtils::ParseFloat(value, 1.0f));
            else if (key == "NormalScale")
                OutMaterial.SetNormalScale(MaterialUtils::ParseFloat(value, 1.0f));
            else if (key == "OcclusionStrength")
                OutMaterial.SetOcclusionStrength(MaterialUtils::ParseFloat(value, 1.0f));
            else if (key == "EmissiveColor")
                OutMaterial.SetEmissiveColor(MaterialUtils::ParseVec3(value, OutMaterial.GetEmissiveColor()));
            else if (key == "EmissiveIntensity" || key == "EmissiveStrength")
                OutMaterial.SetEmissiveIntensity(MaterialUtils::ParseFloat(value, 0.0f));
            else if (key == "AlphaCutoff")
                OutMaterial.SetAlphaCutoff(MaterialUtils::ParseFloat(value, 0.5f));
            else if (key == "DoubleSided")
                OutMaterial.SetDoubleSided(MaterialUtils::ParseBool(value, false));
            else if (key == "AlphaMode") {
                std::string mode = MaterialUtils::CleanValue(value);
                std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
                if (mode == "mask")
                    OutMaterial.SetAlphaMode(EAlphaMode::Mask);
                else if (mode == "blend" || mode == "translucent")
                    OutMaterial.SetAlphaMode(EAlphaMode::Blend);
                else
                    OutMaterial.SetAlphaMode(EAlphaMode::Opaque);
            } else if (key == "UVTiling") {
                glm::vec3 v = MaterialUtils::ParseVec3(value, glm::vec3(1.0f, 1.0f, 0.0f));
                OutMaterial.SetUVTiling(glm::vec2(v.x, v.y));
            } else if (key == "UVOffset") {
                glm::vec3 v = MaterialUtils::ParseVec3(value, glm::vec3(0.0f, 0.0f, 0.0f));
                OutMaterial.SetUVOffset(glm::vec2(v.x, v.y));
            } else if (key == "AlbedoMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetAlbedoMap(UAssetManager::GetTexture2D(path));
            } else if (key == "NormalMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetNormalMap(UAssetManager::GetTexture2D(path));
            } else if (key == "MetallicMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetMetallicMap(UAssetManager::GetTexture2D(path));
            } else if (key == "RoughnessMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetRoughnessMap(UAssetManager::GetTexture2D(path));
            } else if (key == "AOMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetAOMap(UAssetManager::GetTexture2D(path));
            } else if (key == "EmissiveMap") {
                std::string path = MaterialUtils::CleanValue(value);
                OutMaterial.SetEmissiveMap(UAssetManager::GetTexture2D(path));
            } else if (key == "UsePlanarReflection") {
                OutMaterial.SetUsePlanarReflection(MaterialUtils::ParseBool(value, false));
            }
        }

        return true;
    }

} // namespace Leon
