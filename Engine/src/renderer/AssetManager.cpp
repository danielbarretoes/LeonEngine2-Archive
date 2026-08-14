#include "renderer/AssetManager.hpp"
#include "core/Log.hpp"
#include "scene/MaterialSerializer.hpp"

namespace Leon {

    std::unordered_map<std::string, TRef<FTexture2D>> FAssetManager::s_TextureCache;
    std::unordered_map<std::string, TRef<FShader>> FAssetManager::s_ShaderCache;
    std::unordered_map<std::string, TRef<FMaterial>> FAssetManager::s_MaterialCache;
    TRef<FMaterial> FAssetManager::s_DefaultMaterial = nullptr;
    TRef<FTexture2D> FAssetManager::s_DefaultWhiteTexture = nullptr;
    TRef<FTexture2D> FAssetManager::s_DefaultBlackTexture = nullptr;
    TRef<FTexture2D> FAssetManager::s_DefaultFlatNormalTexture = nullptr;

    void FAssetManager::Init() {
        LE_CORE_INFO("Initializing FAssetManager Subsystem...");
        Clear();

        // Initialize 1x1 default fallback textures
        s_DefaultWhiteTexture = FTexture2D::Create(1, 1);
        uint32_t whitePixel = 0xFFFFFFFF;
        s_DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

        s_DefaultBlackTexture = FTexture2D::Create(1, 1);
        uint32_t blackPixel = 0xFF000000;
        s_DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));

        s_DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
        uint32_t flatNormalPixel = 0xFFFF8080; // RGBA: (128, 128, 255, 255) in memory
        s_DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));
    }

    void FAssetManager::Shutdown() {
        LE_CORE_INFO("Shutting down FAssetManager Subsystem...");
        Clear();
    }

    TRef<FTexture2D> FAssetManager::GetDefaultWhiteTexture() {
        if (!s_DefaultWhiteTexture) {
            s_DefaultWhiteTexture = FTexture2D::Create(1, 1);
            uint32_t whitePixel = 0xFFFFFFFF;
            s_DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));
        }
        return s_DefaultWhiteTexture;
    }

    TRef<FTexture2D> FAssetManager::GetDefaultBlackTexture() {
        if (!s_DefaultBlackTexture) {
            s_DefaultBlackTexture = FTexture2D::Create(1, 1);
            uint32_t blackPixel = 0xFF000000;
            s_DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));
        }
        return s_DefaultBlackTexture;
    }

    TRef<FTexture2D> FAssetManager::GetDefaultFlatNormalTexture() {
        if (!s_DefaultFlatNormalTexture) {
            s_DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
            uint32_t flatNormalPixel = 0xFFFF8080;
            s_DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));
        }
        return s_DefaultFlatNormalTexture;
    }

    TRef<FTexture2D> FAssetManager::GetTexture2D(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        auto it = s_TextureCache.find(InPath);
        if (it != s_TextureCache.end() && it->second) {
            return it->second;
        }

        TRef<FTexture2D> texture = FTexture2D::Create(InPath);
        if (texture && texture->IsLoaded()) {
            s_TextureCache[InPath] = texture;
            return texture;
        }

        LE_CORE_WARN("FAssetManager: Failed to load texture from \"{0}\"", InPath);
        return nullptr;
    }

    void FAssetManager::AddTexture2D(const std::string& InName, const TRef<FTexture2D>& InTexture) {
        if (!InName.empty() && InTexture) {
            s_TextureCache[InName] = InTexture;
        }
    }

    bool FAssetManager::HasTexture2D(const std::string& InPath) {
        return s_TextureCache.find(InPath) != s_TextureCache.end();
    }

    TRef<FShader> FAssetManager::GetShader(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        auto it = s_ShaderCache.find(InPath);
        if (it != s_ShaderCache.end() && it->second) {
            return it->second;
        }

        TRef<FShader> shader = FShader::Create(InPath);
        if (shader) {
            s_ShaderCache[InPath] = shader;
            return shader;
        }

        LE_CORE_WARN("FAssetManager: Failed to load shader from \"{0}\"", InPath);
        return nullptr;
    }

    void FAssetManager::AddShader(const std::string& InName, const TRef<FShader>& InShader) {
        if (!InName.empty() && InShader) {
            s_ShaderCache[InName] = InShader;
        }
    }

    bool FAssetManager::HasShader(const std::string& InPath) {
        return s_ShaderCache.find(InPath) != s_ShaderCache.end();
    }

    TRef<FMaterial> FAssetManager::GetMaterial(const std::string& InPath) {
        if (InPath.empty())
            return GetDefaultMaterial();

        auto it = s_MaterialCache.find(InPath);
        if (it != s_MaterialCache.end() && it->second) {
            return it->second;
        }

        auto material = MakeRef<FMaterial>(InPath);
        material->SetAssetPath(InPath);
        if (FMaterialSerializer::Deserialize(InPath, *material)) {
            s_MaterialCache[InPath] = material;
            return material;
        }

        LE_CORE_WARN("FAssetManager: Failed to load material from \"{0}\"", InPath);
        return GetDefaultMaterial();
    }

    void FAssetManager::AddMaterial(const std::string& InName, const TRef<FMaterial>& InMaterial) {
        if (!InName.empty() && InMaterial) {
            s_MaterialCache[InName] = InMaterial;
        }
    }

    bool FAssetManager::HasMaterial(const std::string& InPath) {
        return s_MaterialCache.find(InPath) != s_MaterialCache.end();
    }

    TRef<FMaterial> FAssetManager::GetDefaultMaterial() {
        if (!s_DefaultMaterial) {
            s_DefaultMaterial = FMaterial::Create("M_DefaultPBR");
            s_DefaultMaterial->SetAlbedoColor(glm::vec3(1.0f));
            s_DefaultMaterial->SetMetallic(0.0f);
            s_DefaultMaterial->SetRoughness(0.5f);
            s_DefaultMaterial->SetAO(1.0f);
        }
        return s_DefaultMaterial;
    }

    TRef<FMaterialInstance> FAssetManager::CreateMaterialInstance(const std::string& InMaterialPath) {
        TRef<FMaterial> parentMat = GetMaterial(InMaterialPath);
        if (!parentMat)
            parentMat = GetDefaultMaterial();
        return parentMat->CreateInstance();
    }

    TRef<FMaterialInstance> FAssetManager::CreateMaterialInstance(const TRef<FMaterial>& InParent) {
        if (InParent)
            return InParent->CreateInstance();
        return GetDefaultMaterial()->CreateInstance();
    }

    void FAssetManager::Clear() {
        s_TextureCache.clear();
        s_ShaderCache.clear();
        s_MaterialCache.clear();
        s_DefaultMaterial = nullptr;
        s_DefaultWhiteTexture = nullptr;
        s_DefaultBlackTexture = nullptr;
        s_DefaultFlatNormalTexture = nullptr;
    }

} // namespace Leon
