#include "asset/MaterialImporter.hpp"
#include "asset/AssetPath.hpp"
#include "core/Log.hpp"
#include "scene/MaterialSerializer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Leon {

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

        for (const auto& path : InAvailableTextureVirtualPaths) {
            std::string pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

            bool matchesSlot = (!kw1.empty() && pathLower.find(kw1) != std::string::npos) ||
                               (!kw2.empty() && pathLower.find(kw2) != std::string::npos);

            if (matchesSlot) {
                // If material token is in texture name
                if (pathLower.find(matLower) != std::string::npos) {
                    return path;
                }
            }
        }

        // Second pass: try partial token matching (split by '_')
        std::stringstream ss(matLower);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, '_')) {
            if (!token.empty() && token.length() > 2) {
                tokens.push_back(token);
            }
        }

        for (const auto& path : InAvailableTextureVirtualPaths) {
            std::string pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

            bool matchesSlot = (!kw1.empty() && pathLower.find(kw1) != std::string::npos) ||
                               (!kw2.empty() && pathLower.find(kw2) != std::string::npos);

            if (matchesSlot) {
                for (const auto& t : tokens) {
                    if (pathLower.find(t) != std::string::npos) {
                        return path;
                    }
                }
            }
        }

        return "";
    }

    TRef<FMaterial> FMaterialImporter::BuildMaterial(const FExtractedMaterial& InExtracted,
                                                     const std::vector<std::string>& InAvailableTextureVirtualPaths) {
        std::string matName = InExtracted.Name.empty() ? "M_Extracted" : ("M_" + InExtracted.Name);
        auto material = FMaterial::Create(matName);

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

        // Resolve Textures
        std::string albedoTex = FindMatchingTexture(InExtracted.Name, "diff", "color", InAvailableTextureVirtualPaths);
        if (albedoTex.empty())
            albedoTex = FindMatchingTexture(InExtracted.Name, "albedo", "basecolor", InAvailableTextureVirtualPaths);
        if (!albedoTex.empty()) {
            material->SetTexturePath(0, albedoTex);
        }

        std::string normalTex = FindMatchingTexture(InExtracted.Name, "normal", "nmap", InAvailableTextureVirtualPaths);
        if (!normalTex.empty()) {
            material->SetTexturePath(1, normalTex);
        }

        std::string roughTex = FindMatchingTexture(InExtracted.Name, "gloss", "rough", InAvailableTextureVirtualPaths);
        if (!roughTex.empty()) {
            material->SetTexturePath(3, roughTex);
        }

        std::string metalTex =
            FindMatchingTexture(InExtracted.Name, "metal", "metallic", InAvailableTextureVirtualPaths);
        if (!metalTex.empty()) {
            material->SetTexturePath(2, metalTex);
        }

        std::string aoTex = FindMatchingTexture(InExtracted.Name, "ao", "occlusion", InAvailableTextureVirtualPaths);
        if (!aoTex.empty()) {
            material->SetTexturePath(4, aoTex);
        }

        std::string emissiveTex =
            FindMatchingTexture(InExtracted.Name, "illum", "emissive", InAvailableTextureVirtualPaths);
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
