#include "renderer/AssetManager.hpp"
#include "core/Log.hpp"
#include "scene/MaterialSerializer.hpp"

namespace Leon {

    std::unordered_map<std::string, TRef<FTexture2D>> FAssetManager::s_TextureCache;
    std::unordered_map<std::string, TRef<FShader>> FAssetManager::s_ShaderCache;
    std::unordered_map<std::string, TRef<FPBRMaterial>> FAssetManager::s_MaterialCache;

    void FAssetManager::Init() {
        LE_CORE_INFO("Initializing FAssetManager Subsystem...");
        Clear();
    }

    void FAssetManager::Shutdown() {
        LE_CORE_INFO("Shutting down FAssetManager Subsystem...");
        Clear();
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

    TRef<FPBRMaterial> FAssetManager::GetMaterial(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        auto it = s_MaterialCache.find(InPath);
        if (it != s_MaterialCache.end() && it->second) {
            return it->second;
        }

        auto material = MakeRef<FPBRMaterial>();
        if (FMaterialSerializer::Deserialize(InPath, *material)) {
            s_MaterialCache[InPath] = material;
            return material;
        }

        LE_CORE_WARN("FAssetManager: Failed to load material from \"{0}\"", InPath);
        return nullptr;
    }

    void FAssetManager::AddMaterial(const std::string& InName, const TRef<FPBRMaterial>& InMaterial) {
        if (!InName.empty() && InMaterial) {
            s_MaterialCache[InName] = InMaterial;
        }
    }

    bool FAssetManager::HasMaterial(const std::string& InPath) {
        return s_MaterialCache.find(InPath) != s_MaterialCache.end();
    }

    void FAssetManager::Clear() {
        s_TextureCache.clear();
        s_ShaderCache.clear();
        s_MaterialCache.clear();
    }

} // namespace Leon
