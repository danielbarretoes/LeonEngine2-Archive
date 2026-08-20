#include "Renderer/FMaterialInstance.hpp"
#include "Assets/UAssetManager.hpp"

namespace Leon {

    FMaterialInstance::FMaterialInstance(const TRef<FMaterial>& InParent, const std::string& InName)
        : ParentMaterial(InParent), Name(InName) {}

    TRef<FMaterialInstance> FMaterialInstance::Create(const TRef<FMaterial>& InParent, const std::string& InName) {
        return MakeRef<FMaterialInstance>(InParent, InName);
    }

    glm::vec3 FMaterialInstance::GetAlbedoColor() const {
        if (AlbedoColorOverride.has_value())
            return AlbedoColorOverride.value();
        return ParentMaterial ? ParentMaterial->GetAlbedoColor() : glm::vec3(1.0f);
    }

    float FMaterialInstance::GetMetallic() const {
        if (MetallicOverride.has_value())
            return MetallicOverride.value();
        return ParentMaterial ? ParentMaterial->GetMetallic() : 0.0f;
    }

    float FMaterialInstance::GetRoughness() const {
        if (RoughnessOverride.has_value())
            return RoughnessOverride.value();
        return ParentMaterial ? ParentMaterial->GetRoughness() : 0.5f;
    }

    float FMaterialInstance::GetAO() const {
        if (AOOverride.has_value())
            return AOOverride.value();
        return ParentMaterial ? ParentMaterial->GetAO() : 1.0f;
    }

    float FMaterialInstance::GetNormalScale() const {
        if (NormalScaleOverride.has_value())
            return NormalScaleOverride.value();
        return ParentMaterial ? ParentMaterial->GetNormalScale() : 1.0f;
    }

    float FMaterialInstance::GetOcclusionStrength() const {
        if (OcclusionStrengthOverride.has_value())
            return OcclusionStrengthOverride.value();
        return ParentMaterial ? ParentMaterial->GetOcclusionStrength() : 1.0f;
    }

    glm::vec3 FMaterialInstance::GetEmissiveColor() const {
        if (EmissiveColorOverride.has_value())
            return EmissiveColorOverride.value();
        return ParentMaterial ? ParentMaterial->GetEmissiveColor() : glm::vec3(0.0f);
    }

    float FMaterialInstance::GetEmissiveIntensity() const {
        if (EmissiveIntensityOverride.has_value())
            return EmissiveIntensityOverride.value();
        return ParentMaterial ? ParentMaterial->GetEmissiveIntensity() : 0.0f;
    }

    EAlphaMode FMaterialInstance::GetAlphaMode() const {
        if (AlphaModeOverride.has_value())
            return AlphaModeOverride.value();
        return ParentMaterial ? ParentMaterial->GetAlphaMode() : EAlphaMode::Opaque;
    }

    float FMaterialInstance::GetAlphaCutoff() const {
        if (AlphaCutoffOverride.has_value())
            return AlphaCutoffOverride.value();
        return ParentMaterial ? ParentMaterial->GetAlphaCutoff() : 0.5f;
    }

    bool FMaterialInstance::GetDoubleSided() const {
        if (bDoubleSidedOverride.has_value())
            return bDoubleSidedOverride.value();
        return ParentMaterial ? ParentMaterial->GetDoubleSided() : false;
    }

    glm::vec2 FMaterialInstance::GetUVTiling() const {
        if (UVTilingOverride.has_value())
            return UVTilingOverride.value();
        return ParentMaterial ? ParentMaterial->GetUVTiling() : glm::vec2(1.0f, 1.0f);
    }

    glm::vec2 FMaterialInstance::GetUVOffset() const {
        if (UVOffsetOverride.has_value())
            return UVOffsetOverride.value();
        return ParentMaterial ? ParentMaterial->GetUVOffset() : glm::vec2(0.0f, 0.0f);
    }

    bool FMaterialInstance::GetUsePlanarReflection() const {
        if (bUsePlanarReflectionOverride.has_value())
            return bUsePlanarReflectionOverride.value();
        return ParentMaterial ? ParentMaterial->GetUsePlanarReflection() : false;
    }

    TRef<FTexture2D> FMaterialInstance::GetTexture(uint32_t InSlot) const {
        auto it = TextureOverrides.find(InSlot);
        if (it != TextureOverrides.end() && it->second)
            return it->second;
        return ParentMaterial ? ParentMaterial->GetTexture(InSlot) : nullptr;
    }

    bool FMaterialInstance::HasTexture(uint32_t InSlot) const {
        return GetTexture(InSlot) != nullptr;
    }

    void FMaterialInstance::SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture) {
        if (InTexture)
            TextureOverrides[InSlot] = InTexture;
        else
            TextureOverrides.erase(InSlot);
    }

    void FMaterialInstance::ClearTextureOverride(uint32_t InSlot) {
        TextureOverrides.erase(InSlot);
    }

    void FMaterialInstance::ReloadTextureOverrides() {
        for (auto& [slot, texture] : TextureOverrides) {
            if (!texture)
                continue;
            const std::string& path = texture->GetPath();
            if (path.empty())
                continue;
            texture = UAssetManager::GetTexture2D(path);
        }
    }

    void FMaterialInstance::Bind(const TRef<FShader>& InShader) const {
        if (!InShader)
            return;

        // 1. Resolve Scalar / Vector Parameters
        glm::vec3 albedoColor = GetAlbedoColor();
        float metallic = GetMetallic();
        float roughness = GetRoughness();
        float ao = GetAO();
        float normalScale = GetNormalScale();
        float occlusionStrength = GetOcclusionStrength();
        glm::vec3 emissiveColor = GetEmissiveColor();
        float emissiveIntensity = GetEmissiveIntensity();
        EAlphaMode alphaMode = GetAlphaMode();
        float alphaCutoff = GetAlphaCutoff();
        glm::vec2 uvTiling = GetUVTiling();
        glm::vec2 uvOffset = GetUVOffset();

        InShader->SetFloat3("u_AlbedoColor", albedoColor.r, albedoColor.g, albedoColor.b);
        InShader->SetFloat("u_Metallic", metallic);
        InShader->SetFloat("u_Roughness", roughness);
        InShader->SetFloat("u_AO", ao);
        InShader->SetFloat("u_NormalScale", normalScale);
        InShader->SetFloat("u_OcclusionStrength", occlusionStrength);
        InShader->SetFloat3("u_EmissiveColor", emissiveColor.r, emissiveColor.g, emissiveColor.b);
        InShader->SetFloat("u_EmissiveIntensity", emissiveIntensity);
        InShader->SetInt("u_AlphaMode", static_cast<int>(alphaMode));
        InShader->SetFloat("u_AlphaCutoff", alphaCutoff);
        InShader->SetFloat2("u_UVTiling", uvTiling.x, uvTiling.y);
        InShader->SetFloat2("u_UVOffset", uvOffset.x, uvOffset.y);
        InShader->SetInt("u_DoubleSided", GetDoubleSided() ? 1 : 0);

        // 2. Resolve Textures & Bind to Units (0: Albedo, 1: Normal, 2: Metallic, 3: AO, 4: Roughness, 9: Emissive)
        TRef<FTexture2D> albedoMap = GetTexture(0);
        TRef<FTexture2D> normalMap = GetTexture(1);
        TRef<FTexture2D> metallicMap = GetTexture(2);
        TRef<FTexture2D> aoMap = GetTexture(3);
        TRef<FTexture2D> roughnessMap = GetTexture(4);
        TRef<FTexture2D> emissiveMap = GetTexture(5);
        if (!emissiveMap)
            emissiveMap = GetTexture(9);

        if (albedoMap && albedoMap->IsLoaded()) {
            albedoMap->Bind(0);
            InShader->SetInt("u_UseAlbedoMap", 1);
        } else {
            UAssetManager::GetDefaultWhiteTexture()->Bind(0);
            InShader->SetInt("u_UseAlbedoMap", 0);
        }

        if (normalMap && normalMap->IsLoaded()) {
            normalMap->Bind(1);
            InShader->SetInt("u_UseNormalMap", 1);
        } else {
            UAssetManager::GetDefaultFlatNormalTexture()->Bind(1);
            InShader->SetInt("u_UseNormalMap", 0);
        }

        if (metallicMap && metallicMap->IsLoaded()) {
            metallicMap->Bind(2);
            InShader->SetInt("u_UseMetallicMap", 1);
        } else {
            UAssetManager::GetDefaultWhiteTexture()->Bind(2);
            InShader->SetInt("u_UseMetallicMap", 0);
        }

        if (aoMap && aoMap->IsLoaded()) {
            aoMap->Bind(3);
            InShader->SetInt("u_UseAOMap", 1);
        } else {
            UAssetManager::GetDefaultWhiteTexture()->Bind(3);
            InShader->SetInt("u_UseAOMap", 0);
        }

        if (roughnessMap && roughnessMap->IsLoaded()) {
            roughnessMap->Bind(4);
            InShader->SetInt("u_UseRoughnessMap", 1);
        } else {
            UAssetManager::GetDefaultWhiteTexture()->Bind(4);
            InShader->SetInt("u_UseRoughnessMap", 0);
        }

        if (emissiveMap && emissiveMap->IsLoaded()) {
            emissiveMap->Bind(9);
            InShader->SetInt("u_UseEmissiveMap", 1);
        } else {
            UAssetManager::GetDefaultBlackTexture()->Bind(9);
            InShader->SetInt("u_UseEmissiveMap", 0);
        }
    }

} // namespace Leon
