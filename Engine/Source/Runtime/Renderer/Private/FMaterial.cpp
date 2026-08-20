#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Assets/UAssetManager.hpp"

namespace Leon {

    FMaterial::FMaterial(const std::string& InName, const TRef<FShader>& InShader) : Name(InName), Shader(InShader) {
        if (!Shader) {
            Shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
        }
    }

    TRef<FMaterial> FMaterial::Create(const std::string& InName, const TRef<FShader>& InShader) {
        return MakeRef<FMaterial>(InName, InShader);
    }

    TRef<FMaterialInstance> FMaterial::CreateInstance(const std::string& InInstanceName) {
        std::string name = InInstanceName.empty() ? (Name + "_Inst") : InInstanceName;
        return MakeRef<FMaterialInstance>(shared_from_this(), name);
    }

    void FMaterial::SetAlphaMode(EAlphaMode InMode) {
        AlphaMode = InMode;
        if (AlphaMode == EAlphaMode::Blend) {
            PipelineState.bBlend = true;
            PipelineState.SrcBlend = EBlendFactor::SrcAlpha;
            PipelineState.DstBlend = EBlendFactor::OneMinusSrcAlpha;
            PipelineState.bDepthWrite = false;
        } else {
            PipelineState.bBlend = false;
            PipelineState.bDepthWrite = true;
        }
    }

    void FMaterial::SetDoubleSided(bool bDouble) {
        bDoubleSided = bDouble;
        PipelineState.CullMode = bDouble ? ECullMode::None : ECullMode::Back;
    }

    TRef<FTexture2D> FMaterial::GetTexture(uint32_t InSlot) const {
        switch (InSlot) {
        case 0:
            return AlbedoMap;
        case 1:
            return NormalMap;
        case 2:
            return MetallicMap;
        case 3:
            return AOMap;
        case 4:
            return RoughnessMap;
        case 5:
        case 9:
            return EmissiveMap;
        default:
            return nullptr;
        }
    }

    void FMaterial::SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture) {
        switch (InSlot) {
        case 0:
            SetAlbedoMap(InTexture);
            break;
        case 1:
            SetNormalMap(InTexture);
            break;
        case 2:
            SetMetallicMap(InTexture);
            break;
        case 3:
            SetAOMap(InTexture);
            break;
        case 4:
            SetRoughnessMap(InTexture);
            break;
        case 5:
        case 9:
            SetEmissiveMap(InTexture);
            break;
        default:
            break;
        }
    }

    void FMaterial::ReloadTextures() {
        for (uint32_t slot = 0; slot < 12; ++slot) {
            std::string path = GetTexturePath(slot);
            if (path.empty()) {
                if (const TRef<FTexture2D> existing = GetTexture(slot))
                    path = existing->GetPath();
            }
            if (path.empty())
                continue;
            SetTexturePath(slot, path);
            SetTexture(slot, UAssetManager::GetTexture2D(path));
        }
    }

} // namespace Leon
