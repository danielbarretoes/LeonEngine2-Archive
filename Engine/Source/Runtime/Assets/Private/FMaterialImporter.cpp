#include "Assets/FMaterialImporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"
#include "Engine/FMaterialSerializer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Leon {

    static std::string ResolveTextureName(const std::string& InTexName,
                                          const std::vector<std::string>& InAvailableTextureVirtualPaths) {
        if (InTexName.empty())
            return "";

        std::string targetStem = FAssetPath::GetFileNameWithoutExtension(InTexName);
        std::string targetStemLower = targetStem;
        std::transform(targetStemLower.begin(), targetStemLower.end(), targetStemLower.begin(), ::tolower);

        for (const auto& path : InAvailableTextureVirtualPaths) {
            std::string pathStem = FAssetPath::GetFileNameWithoutExtension(path);
            std::string pathStemLower = pathStem;
            std::transform(pathStemLower.begin(), pathStemLower.end(), pathStemLower.begin(), ::tolower);

            if (pathStemLower == targetStemLower) {
                return path;
            }
        }
        return "";
    }

    static bool MatchSlotKeyword(const std::string& pathLower, const std::string& kw) {
        if (kw.empty())
            return false;
        if (kw.length() <= 3 && kw[0] == '_') {
            // Suffix pattern like "_d", "_n", "_r", "_m", "_e", "_ao"
            // Must be followed by '.', '_', or end of string
            size_t pos = pathLower.find(kw);
            while (pos != std::string::npos) {
                size_t nextPos = pos + kw.length();
                if (nextPos >= pathLower.length() || pathLower[nextPos] == '.' || pathLower[nextPos] == '_') {
                    return true;
                }
                pos = pathLower.find(kw, pos + 1);
            }
            return false;
        }
        return pathLower.find(kw) != std::string::npos;
    }

    std::string FMaterialImporter::FindMatchingTexture(const std::string& InMaterialName,
                                                       const std::string& InSlotKeyword1,
                                                       const std::string& InSlotKeyword2,
                                                       const std::vector<std::string>& InAvailableTextureVirtualPaths) {
        std::string matLower = InMaterialName;
        std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

        std::string kw1 = InSlotKeyword1;
        std::transform(kw1.begin(), kw1.end(), kw1.begin(), ::tolower);

        std::string kw2 = InSlotKeyword2;
        std::transform(kw2.begin(), kw2.end(), kw2.begin(), ::tolower);

        // Normalize material name: strip prefix "M_" or "MI_" or "Material_" if present
        if (matLower.find("m_") == 0)
            matLower = matLower.substr(2);
        if (matLower.find("mi_") == 0)
            matLower = matLower.substr(3);
        if (matLower.find("material_") == 0)
            matLower = matLower.substr(9);

        // First pass: look for exact slot keyword + material name match
        for (const auto& path : InAvailableTextureVirtualPaths) {
            std::string pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

            bool matchesSlot = MatchSlotKeyword(pathLower, kw1) || MatchSlotKeyword(pathLower, kw2);

            if (matchesSlot) {
                // If full material token is in texture name
                if (pathLower.find(matLower) != std::string::npos) {
                    return path;
                }
            }
        }

        // Second pass: split material name into sub-tokens (e.g. "car_body" -> "car", "body")
        std::stringstream ss(matLower);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, '_')) {
            if (!token.empty() && token.length() >= 2) {
                tokens.push_back(token);
            }
        }

        for (const auto& path : InAvailableTextureVirtualPaths) {
            std::string pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

            bool matchesSlot = MatchSlotKeyword(pathLower, kw1) || MatchSlotKeyword(pathLower, kw2);

            if (matchesSlot) {
                bool allTokensMatch = !tokens.empty();
                for (const auto& t : tokens) {
                    if (pathLower.find(t) == std::string::npos) {
                        allTokensMatch = false;
                        break;
                    }
                }
                if (allTokensMatch) {
                    return path;
                }
            }
        }

        return "";
    }

    TRef<FMaterial> FMaterialImporter::BuildMaterial(const FExtractedMaterial& InExtracted,
                                                     const std::vector<std::string>& InAvailableTextureVirtualPaths) {
        std::string formattedName = InExtracted.Name;
        if (formattedName.rfind("M_", 0) != 0) {
            formattedName = "M_" + formattedName;
        }
        auto material = FMaterial::Create(formattedName);

        material->SetAlbedoColor(InExtracted.BaseColor);
        material->SetMetallic(InExtracted.Metallic);
        material->SetRoughness(InExtracted.Roughness);
        material->SetAO(InExtracted.AO);
        material->SetNormalScale(InExtracted.NormalScale);
        material->SetEmissiveColor(InExtracted.EmissiveColor);
        material->SetEmissiveIntensity(InExtracted.EmissiveIntensity);
        material->SetAlphaMode(InExtracted.AlphaMode);
        material->SetAlphaCutoff(InExtracted.AlphaCutoff);
        material->SetDoubleSided(InExtracted.bDoubleSided);

        std::string nameLower = InExtracted.Name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        // Auto planar reflection for glass, windows, mirrors, chrome
        if (nameLower.find("glass") != std::string::npos || nameLower.find("window") != std::string::npos ||
            nameLower.find("chrome") != std::string::npos || nameLower.find("mirror") != std::string::npos ||
            (InExtracted.Metallic >= 0.85f && InExtracted.Roughness <= 0.1f)) {
            material->SetUsePlanarReflection(true);
        }

        // 1. Resolve Albedo / Diffuse Map
        std::string albedoTex = ResolveTextureName(InExtracted.DiffuseTextureName, InAvailableTextureVirtualPaths);
        if (albedoTex.empty())
            albedoTex = FindMatchingTexture(InExtracted.Name, "diff", "color", InAvailableTextureVirtualPaths);
        if (albedoTex.empty())
            albedoTex = FindMatchingTexture(InExtracted.Name, "albedo", "basecolor", InAvailableTextureVirtualPaths);
        if (albedoTex.empty())
            albedoTex = FindMatchingTexture(InExtracted.Name, "_d", "diffuse", InAvailableTextureVirtualPaths);
        if (!albedoTex.empty()) {
            material->SetTexturePath(0, albedoTex);
        }

        // 2. Resolve Normal Map
        std::string normalTex = ResolveTextureName(InExtracted.NormalTextureName, InAvailableTextureVirtualPaths);
        if (normalTex.empty())
            normalTex = FindMatchingTexture(InExtracted.Name, "normal", "nmap", InAvailableTextureVirtualPaths);
        if (normalTex.empty())
            normalTex = FindMatchingTexture(InExtracted.Name, "_n", "norm", InAvailableTextureVirtualPaths);
        if (!normalTex.empty()) {
            material->SetTexturePath(1, normalTex);
        }

        // 3. Resolve Metallic Map
        std::string metalTex = ResolveTextureName(InExtracted.MetallicTextureName, InAvailableTextureVirtualPaths);
        if (metalTex.empty())
            metalTex = FindMatchingTexture(InExtracted.Name, "metal", "metallic", InAvailableTextureVirtualPaths);
        if (metalTex.empty())
            metalTex = FindMatchingTexture(InExtracted.Name, "_m", "met", InAvailableTextureVirtualPaths);
        if (!metalTex.empty()) {
            material->SetTexturePath(2, metalTex);
        }

        // 4. Resolve Roughness Map
        std::string roughTex = ResolveTextureName(InExtracted.RoughnessTextureName, InAvailableTextureVirtualPaths);
        if (roughTex.empty())
            roughTex = FindMatchingTexture(InExtracted.Name, "gloss", "rough", InAvailableTextureVirtualPaths);
        if (roughTex.empty())
            roughTex = FindMatchingTexture(InExtracted.Name, "_r", "roughness", InAvailableTextureVirtualPaths);
        if (!roughTex.empty()) {
            material->SetTexturePath(3, roughTex);
        }

        // 5. Resolve AO Map
        std::string aoTex = ResolveTextureName(InExtracted.AOTextureName, InAvailableTextureVirtualPaths);
        if (aoTex.empty())
            aoTex = FindMatchingTexture(InExtracted.Name, "ao", "occlusion", InAvailableTextureVirtualPaths);
        if (aoTex.empty())
            aoTex = FindMatchingTexture(InExtracted.Name, "_ao", "ambient", InAvailableTextureVirtualPaths);
        if (!aoTex.empty()) {
            material->SetTexturePath(4, aoTex);
        }

        // 6. Resolve Emissive Map
        std::string emissiveTex = ResolveTextureName(InExtracted.EmissiveTextureName, InAvailableTextureVirtualPaths);
        if (emissiveTex.empty())
            emissiveTex = FindMatchingTexture(InExtracted.Name, "illum", "emissive", InAvailableTextureVirtualPaths);
        if (emissiveTex.empty())
            emissiveTex = FindMatchingTexture(InExtracted.Name, "_e", "emit", InAvailableTextureVirtualPaths);
        if (!emissiveTex.empty()) {
            material->SetTexturePath(5, emissiveTex);
            if (material->GetEmissiveIntensity() <= 0.0f) {
                material->SetEmissiveIntensity(1.0f);
                material->SetEmissiveColor(glm::vec3(1.0f));
            }
        }

        return material;
    }

    bool FMaterialImporter::SaveMaterialToFile(const TRef<FMaterial>& InMaterial,
                                               const std::string& InDestinationLMatPath) {
        if (!InMaterial)
            return false;

        return FMaterialSerializer::Serialize(InDestinationLMatPath, *InMaterial);
    }

    bool FMaterialImporter::SaveMaterialInstanceToFile(const TRef<FMaterialInstance>& InInstance,
                                                       const std::string& InDestinationLMiPath,
                                                       const std::string& InParentVirtualPath) {
        if (!InInstance)
            return false;

        std::ofstream file(InDestinationLMiPath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FMaterialImporter: Failed to open \"{0}\" for writing", InDestinationLMiPath);
            return false;
        }

        file << "# LeonEngine2 Material Instance Asset File (.lmi)\n";
        file << "MaterialInstance:\n";
        file << "  Parent: \"" << InParentVirtualPath << "\"\n";
        file << "  Overrides:\n";

        if (InInstance->GetRoughness() != 0.5f) {
            file << "    Roughness: " << InInstance->GetRoughness() << "\n";
        }
        if (InInstance->GetMetallic() != 0.0f) {
            file << "    Metallic: " << InInstance->GetMetallic() << "\n";
        }
        if (InInstance->GetNormalScale() != 1.0f) {
            file << "    NormalScale: " << InInstance->GetNormalScale() << "\n";
        }

        return file.good();
    }

} // namespace Leon
