#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/AssetManager.hpp"

namespace Leon {

    FMaterial::FMaterial(const std::string& InName, const TRef<FShader>& InShader)
        : m_Name(InName), m_Shader(InShader) {
        if (!m_Shader) {
            m_Shader = FAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        }
    }

    TRef<FMaterial> FMaterial::Create(const std::string& InName, const TRef<FShader>& InShader) {
        return MakeRef<FMaterial>(InName, InShader);
    }

    TRef<FMaterialInstance> FMaterial::CreateInstance(const std::string& InInstanceName) {
        std::string name = InInstanceName.empty() ? (m_Name + "_Inst") : InInstanceName;
        return MakeRef<FMaterialInstance>(shared_from_this(), name);
    }

    void FMaterial::SetAlphaMode(EAlphaMode InMode) {
        m_AlphaMode = InMode;
        if (m_AlphaMode == EAlphaMode::Blend) {
            m_PipelineState.bBlend = true;
            m_PipelineState.SrcBlend = EBlendFactor::SrcAlpha;
            m_PipelineState.DstBlend = EBlendFactor::OneMinusSrcAlpha;
            m_PipelineState.bDepthWrite = false;
        } else {
            m_PipelineState.bBlend = false;
            m_PipelineState.bDepthWrite = true;
        }
    }

    void FMaterial::SetDoubleSided(bool bDouble) {
        m_bDoubleSided = bDouble;
        m_PipelineState.CullMode = bDouble ? ECullMode::None : ECullMode::Back;
    }

    TRef<FTexture2D> FMaterial::GetTexture(uint32_t InSlot) const {
        switch (InSlot) {
            case 0: return m_AlbedoMap;
            case 1: return m_NormalMap;
            case 2: return m_MetallicMap;
            case 3: return m_AOMap;
            case 4: return m_RoughnessMap;
            case 5:
            case 9: return m_EmissiveMap;
            default: return nullptr;
        }
    }

    void FMaterial::SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture) {
        switch (InSlot) {
            case 0: SetAlbedoMap(InTexture); break;
            case 1: SetNormalMap(InTexture); break;
            case 2: SetMetallicMap(InTexture); break;
            case 3: SetAOMap(InTexture); break;
            case 4: SetRoughnessMap(InTexture); break;
            case 5:
            case 9: SetEmissiveMap(InTexture); break;
            default: break;
        }
    }

} // namespace Leon
