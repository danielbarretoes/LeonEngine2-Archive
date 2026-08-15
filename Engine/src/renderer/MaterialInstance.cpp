#include "renderer/MaterialInstance.hpp"
#include "renderer/AssetManager.hpp"

namespace Leon {

    FMaterialInstance::FMaterialInstance(const TRef<FMaterial>& InParent, const std::string& InName)
        : m_ParentMaterial(InParent), m_Name(InName) {}

    TRef<FMaterialInstance> FMaterialInstance::Create(const TRef<FMaterial>& InParent, const std::string& InName) {
        return MakeRef<FMaterialInstance>(InParent, InName);
    }

    glm::vec3 FMaterialInstance::GetAlbedoColor() const {
        if (m_AlbedoColorOverride.has_value())
            return m_AlbedoColorOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetAlbedoColor() : glm::vec3(1.0f);
    }

    float FMaterialInstance::GetMetallic() const {
        if (m_MetallicOverride.has_value())
            return m_MetallicOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetMetallic() : 0.0f;
    }

    float FMaterialInstance::GetRoughness() const {
        if (m_RoughnessOverride.has_value())
            return m_RoughnessOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetRoughness() : 0.5f;
    }

    float FMaterialInstance::GetAO() const {
        if (m_AOOverride.has_value())
            return m_AOOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetAO() : 1.0f;
    }

    float FMaterialInstance::GetNormalScale() const {
        if (m_NormalScaleOverride.has_value())
            return m_NormalScaleOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetNormalScale() : 1.0f;
    }

    float FMaterialInstance::GetOcclusionStrength() const {
        if (m_OcclusionStrengthOverride.has_value())
            return m_OcclusionStrengthOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetOcclusionStrength() : 1.0f;
    }

    glm::vec3 FMaterialInstance::GetEmissiveColor() const {
        if (m_EmissiveColorOverride.has_value())
            return m_EmissiveColorOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetEmissiveColor() : glm::vec3(0.0f);
    }

    float FMaterialInstance::GetEmissiveIntensity() const {
        if (m_EmissiveIntensityOverride.has_value())
            return m_EmissiveIntensityOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetEmissiveIntensity() : 0.0f;
    }

    EAlphaMode FMaterialInstance::GetAlphaMode() const {
        if (m_AlphaModeOverride.has_value())
            return m_AlphaModeOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetAlphaMode() : EAlphaMode::Opaque;
    }

    float FMaterialInstance::GetAlphaCutoff() const {
        if (m_AlphaCutoffOverride.has_value())
            return m_AlphaCutoffOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetAlphaCutoff() : 0.5f;
    }

    bool FMaterialInstance::GetDoubleSided() const {
        if (m_bDoubleSidedOverride.has_value())
            return m_bDoubleSidedOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetDoubleSided() : false;
    }

    glm::vec2 FMaterialInstance::GetUVTiling() const {
        if (m_UVTilingOverride.has_value())
            return m_UVTilingOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetUVTiling() : glm::vec2(1.0f, 1.0f);
    }

    glm::vec2 FMaterialInstance::GetUVOffset() const {
        if (m_UVOffsetOverride.has_value())
            return m_UVOffsetOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetUVOffset() : glm::vec2(0.0f, 0.0f);
    }

    bool FMaterialInstance::GetUsePlanarReflection() const {
        if (m_bUsePlanarReflectionOverride.has_value())
            return m_bUsePlanarReflectionOverride.value();
        return m_ParentMaterial ? m_ParentMaterial->GetUsePlanarReflection() : false;
    }

    TRef<FTexture2D> FMaterialInstance::GetTexture(uint32_t InSlot) const {
        auto it = m_TextureOverrides.find(InSlot);
        if (it != m_TextureOverrides.end() && it->second)
            return it->second;
        return m_ParentMaterial ? m_ParentMaterial->GetTexture(InSlot) : nullptr;
    }

    bool FMaterialInstance::HasTexture(uint32_t InSlot) const {
        return GetTexture(InSlot) != nullptr;
    }

    void FMaterialInstance::SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture) {
        if (InTexture)
            m_TextureOverrides[InSlot] = InTexture;
        else
            m_TextureOverrides.erase(InSlot);
    }

    void FMaterialInstance::ClearTextureOverride(uint32_t InSlot) {
        m_TextureOverrides.erase(InSlot);
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
            FAssetManager::GetDefaultWhiteTexture()->Bind(0);
            InShader->SetInt("u_UseAlbedoMap", 0);
        }

        if (normalMap && normalMap->IsLoaded()) {
            normalMap->Bind(1);
            InShader->SetInt("u_UseNormalMap", 1);
        } else {
            FAssetManager::GetDefaultFlatNormalTexture()->Bind(1);
            InShader->SetInt("u_UseNormalMap", 0);
        }

        if (metallicMap && metallicMap->IsLoaded()) {
            metallicMap->Bind(2);
            InShader->SetInt("u_UseMetallicMap", 1);
        } else {
            FAssetManager::GetDefaultWhiteTexture()->Bind(2);
            InShader->SetInt("u_UseMetallicMap", 0);
        }

        if (aoMap && aoMap->IsLoaded()) {
            aoMap->Bind(3);
            InShader->SetInt("u_UseAOMap", 1);
        } else {
            FAssetManager::GetDefaultWhiteTexture()->Bind(3);
            InShader->SetInt("u_UseAOMap", 0);
        }

        if (roughnessMap && roughnessMap->IsLoaded()) {
            roughnessMap->Bind(4);
            InShader->SetInt("u_UseRoughnessMap", 1);
        } else {
            FAssetManager::GetDefaultWhiteTexture()->Bind(4);
            InShader->SetInt("u_UseRoughnessMap", 0);
        }

        if (emissiveMap && emissiveMap->IsLoaded()) {
            emissiveMap->Bind(9);
            InShader->SetInt("u_UseEmissiveMap", 1);
        } else {
            FAssetManager::GetDefaultBlackTexture()->Bind(9);
            InShader->SetInt("u_UseEmissiveMap", 0);
        }
    }

} // namespace Leon
