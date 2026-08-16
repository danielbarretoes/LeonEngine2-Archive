#include "Assets/UAssetManager.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"
#include "Engine/FMaterialSerializer.hpp"
#include "RHI/IRenderDriver.hpp"

#include "Core/FProjectPaths.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Leon {

    std::string UAssetManager::ContentRoot = "";
    std::unordered_map<std::string, TRef<FTexture2D>> UAssetManager::TextureCache;
    std::unordered_map<std::string, TRef<UStaticMesh>> UAssetManager::StaticMeshCache;
    std::unordered_map<std::string, TRef<FLightmapAsset>> UAssetManager::LightmapCache;
    std::unordered_map<std::string, TRef<FShader>> UAssetManager::ShaderCache;
    std::unordered_map<std::string, TRef<FMaterial>> UAssetManager::MaterialCache;
    std::unordered_map<std::string, TRef<FMaterialInstance>> UAssetManager::MaterialInstanceCache;
    TRef<FMaterial> UAssetManager::DefaultMaterial = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultWhiteTexture = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultBlackTexture = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultFlatNormalTexture = nullptr;

    void UAssetManager::Init() {
        LE_CORE_INFO("Initializing UAssetManager Subsystem...");
        Clear();

        // Initialize 1x1 default fallback textures (null when offline / no RenderDriver)
        DefaultWhiteTexture = FTexture2D::Create(1, 1);
        if (DefaultWhiteTexture) {
            uint32_t whitePixel = 0xFFFFFFFF;
            DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));
        }

        DefaultBlackTexture = FTexture2D::Create(1, 1);
        if (DefaultBlackTexture) {
            uint32_t blackPixel = 0xFF000000;
            DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));
        }

        DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
        if (DefaultFlatNormalTexture) {
            uint32_t flatNormalPixel = 0xFFFF8080; // RGBA: (128, 128, 255, 255) in memory
            DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));
        }
    }

    void UAssetManager::Shutdown() {
        LE_CORE_INFO("Shutting down UAssetManager Subsystem...");
        Clear();
    }

    void UAssetManager::SetContentRoot(const std::string& InRoot) {
        ContentRoot = FAssetPath::Normalize(InRoot);
    }

    const std::string& UAssetManager::GetContentRoot() {
        return ContentRoot;
    }

    std::string UAssetManager::ResolveVirtualPath(const std::string& InVirtualPath) {
        if (InVirtualPath.empty())
            return "";

        // Delegate to unified FProjectPaths virtual resolver
        return FProjectPaths::ResolveVirtualPath(InVirtualPath);
    }

    TRef<FTexture2D> UAssetManager::GetDefaultWhiteTexture() {
        if (!DefaultWhiteTexture) {
            DefaultWhiteTexture = FTexture2D::Create(1, 1);
            uint32_t whitePixel = 0xFFFFFFFF;
            DefaultWhiteTexture->SetData(&whitePixel, sizeof(uint32_t));
        }
        return DefaultWhiteTexture;
    }

    TRef<FTexture2D> UAssetManager::GetDefaultBlackTexture() {
        if (!DefaultBlackTexture) {
            DefaultBlackTexture = FTexture2D::Create(1, 1);
            uint32_t blackPixel = 0xFF000000;
            DefaultBlackTexture->SetData(&blackPixel, sizeof(uint32_t));
        }
        return DefaultBlackTexture;
    }

    TRef<FTexture2D> UAssetManager::GetDefaultFlatNormalTexture() {
        if (!DefaultFlatNormalTexture) {
            DefaultFlatNormalTexture = FTexture2D::Create(1, 1);
            uint32_t flatNormalPixel = 0xFFFF8080;
            DefaultFlatNormalTexture->SetData(&flatNormalPixel, sizeof(uint32_t));
        }
        return DefaultFlatNormalTexture;
    }

    TRef<FTexture2D> UAssetManager::GetTexture2D(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = TextureCache.find(resolved);
        if (it != TextureCache.end() && it->second) {
            return it->second;
        }

        TRef<FTexture2D> texture = FTexture2D::Create(resolved);
        if (texture && texture->IsLoaded()) {
            TextureCache[resolved] = texture;
            if (resolved != InPath) {
                TextureCache[InPath] = texture;
            }
            return texture;
        }

        LE_CORE_WARN("UAssetManager: Failed to load texture from \"{0}\"", InPath);
        return nullptr;
    }

    TRef<FTexture2D> UAssetManager::GetHDRTexture(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = TextureCache.find(resolved);
        if (it != TextureCache.end() && it->second) {
            return it->second;
        }

        TRef<FTexture2D> texture = FTexture2D::Create(resolved);
        if (texture && texture->IsLoaded()) {
            TextureCache[resolved] = texture;
            if (resolved != InPath) {
                TextureCache[InPath] = texture;
            }
            return texture;
        }

        LE_CORE_WARN("UAssetManager: Failed to load HDR texture from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddTexture2D(const std::string& InName, const TRef<FTexture2D>& InTexture) {
        if (!InName.empty() && InTexture) {
            TextureCache[InName] = InTexture;
        }
    }

    bool UAssetManager::HasTexture2D(const std::string& InPath) {
        return TextureCache.find(InPath) != TextureCache.end();
    }

    TRef<UStaticMesh> UAssetManager::GetStaticMesh(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = StaticMeshCache.find(resolved);
        if (it != StaticMeshCache.end() && it->second) {
            return it->second;
        }

        auto mesh = UStaticMesh::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (mesh->LoadFromFile(resolved)) {
            if (FRenderDriverRegistry::GetActiveDriver())
                mesh->CreateGPUResources();
            StaticMeshCache[resolved] = mesh;
            if (resolved != InPath) {
                StaticMeshCache[InPath] = mesh;
            }
            return mesh;
        }

        LE_CORE_WARN("UAssetManager: Failed to load static mesh from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddStaticMesh(const std::string& InName, const TRef<UStaticMesh>& InMesh) {
        if (!InName.empty() && InMesh) {
            StaticMeshCache[InName] = InMesh;
        }
    }

    bool UAssetManager::HasStaticMesh(const std::string& InPath) {
        return StaticMeshCache.find(InPath) != StaticMeshCache.end();
    }

    TRef<FLightmapAsset> UAssetManager::GetLightmap(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);
        auto it = LightmapCache.find(resolved);
        if (it != LightmapCache.end() && it->second)
            return it->second;

        auto lightmap = MakeRef<FLightmapAsset>();
        if (lightmap->LoadFromFile(resolved)) {
            LightmapCache[resolved] = lightmap;
            if (resolved != InPath)
                LightmapCache[InPath] = lightmap;
            return lightmap;
        }

        LE_CORE_WARN("UAssetManager: Failed to load lightmap from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddLightmap(const std::string& InName, const TRef<FLightmapAsset>& InLightmap) {
        if (!InName.empty() && InLightmap)
            LightmapCache[InName] = InLightmap;
    }

    bool UAssetManager::HasLightmap(const std::string& InPath) {
        return LightmapCache.find(InPath) != LightmapCache.end();
    }

    TRef<FShader> UAssetManager::GetShader(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = ShaderCache.find(resolved);
        if (it != ShaderCache.end() && it->second) {
            return it->second;
        }

        TRef<FShader> shader = FShader::Create(resolved);
        if (shader) {
            ShaderCache[resolved] = shader;
            if (resolved != InPath) {
                ShaderCache[InPath] = shader;
            }
            return shader;
        }

        LE_CORE_WARN("UAssetManager: Failed to load shader from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddShader(const std::string& InName, const TRef<FShader>& InShader) {
        if (!InName.empty() && InShader) {
            ShaderCache[InName] = InShader;
        }
    }

    bool UAssetManager::HasShader(const std::string& InPath) {
        return ShaderCache.find(InPath) != ShaderCache.end();
    }

    TRef<FMaterial> UAssetManager::GetMaterial(const std::string& InPath) {
        if (InPath.empty())
            return GetDefaultMaterial();

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = MaterialCache.find(resolved);
        if (it != MaterialCache.end() && it->second) {
            return it->second;
        }

        auto material = MakeRef<FMaterial>(InPath);
        material->SetAssetPath(resolved);
        if (FMaterialSerializer::Deserialize(resolved, *material)) {
            MaterialCache[resolved] = material;
            if (resolved != InPath) {
                MaterialCache[InPath] = material;
            }
            return material;
        }

        LE_CORE_WARN("UAssetManager: Failed to load material from \"{0}\"", InPath);
        return GetDefaultMaterial();
    }

    void UAssetManager::AddMaterial(const std::string& InName, const TRef<FMaterial>& InMaterial) {
        if (!InName.empty() && InMaterial) {
            MaterialCache[InName] = InMaterial;
        }
    }

    bool UAssetManager::HasMaterial(const std::string& InPath) {
        return MaterialCache.find(InPath) != MaterialCache.end();
    }

    TRef<FMaterialInstance> UAssetManager::GetMaterialInstance(const std::string& InPath) {
        if (InPath.empty())
            return GetDefaultMaterial()->CreateInstance();

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = MaterialInstanceCache.find(resolved);
        if (it != MaterialInstanceCache.end() && it->second) {
            return it->second;
        }

        // If path ends with .lmat, load parent material and create instance
        if (FAssetPath::GetExtension(resolved) == "lmat") {
            auto parentMat = GetMaterial(resolved);
            auto instance = parentMat->CreateInstance();
            MaterialInstanceCache[resolved] = instance;
            return instance;
        }

        // Parse .lmi text file
        std::ifstream file(resolved);
        if (file.is_open()) {
            std::string line;
            std::string parentPath;
            float roughness = -1.0f;
            float metallic = -1.0f;
            float normalScale = -1.0f;

            while (std::getline(file, line)) {
                size_t pPos = line.find("Parent:");
                if (pPos != std::string::npos) {
                    size_t q1 = line.find('"', pPos);
                    size_t q2 = line.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos) {
                        parentPath = line.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
                size_t rPos = line.find("Roughness:");
                if (rPos != std::string::npos) {
                    roughness = std::stof(line.substr(rPos + 10));
                }
                size_t mPos = line.find("Metallic:");
                if (mPos != std::string::npos) {
                    metallic = std::stof(line.substr(mPos + 9));
                }
                size_t nPos = line.find("NormalScale:");
                if (nPos != std::string::npos) {
                    normalScale = std::stof(line.substr(nPos + 12));
                }
            }

            auto parentMat = GetMaterial(parentPath);
            auto instance = parentMat->CreateInstance();
            if (roughness >= 0.0f)
                instance->SetRoughness(roughness);
            if (metallic >= 0.0f)
                instance->SetMetallic(metallic);
            if (normalScale >= 0.0f)
                instance->SetNormalScale(normalScale);

            MaterialInstanceCache[resolved] = instance;
            return instance;
        }

        return GetDefaultMaterial()->CreateInstance();
    }

    void UAssetManager::AddMaterialInstance(const std::string& InName, const TRef<FMaterialInstance>& InInstance) {
        if (!InName.empty() && InInstance) {
            MaterialInstanceCache[InName] = InInstance;
        }
    }

    bool UAssetManager::HasMaterialInstance(const std::string& InPath) {
        return MaterialInstanceCache.find(InPath) != MaterialInstanceCache.end();
    }

    TRef<FMaterial> UAssetManager::GetDefaultMaterial() {
        if (!DefaultMaterial) {
            DefaultMaterial = FMaterial::Create("M_DefaultPBR");
            DefaultMaterial->SetAlbedoColor(glm::vec3(1.0f));
            DefaultMaterial->SetMetallic(0.0f);
            DefaultMaterial->SetRoughness(0.5f);
            DefaultMaterial->SetAO(1.0f);
        }
        return DefaultMaterial;
    }

    TRef<FMaterialInstance> UAssetManager::CreateMaterialInstance(const std::string& InMaterialPath) {
        TRef<FMaterial> parentMat = GetMaterial(InMaterialPath);
        if (!parentMat)
            parentMat = GetDefaultMaterial();
        return parentMat->CreateInstance();
    }

    TRef<FMaterialInstance> UAssetManager::CreateMaterialInstance(const TRef<FMaterial>& InParent) {
        if (InParent)
            return InParent->CreateInstance();
        return GetDefaultMaterial()->CreateInstance();
    }

    void UAssetManager::Clear() {
        TextureCache.clear();
        StaticMeshCache.clear();
        LightmapCache.clear();
        ShaderCache.clear();
        MaterialCache.clear();
        MaterialInstanceCache.clear();
        DefaultMaterial = nullptr;
        DefaultWhiteTexture = nullptr;
        DefaultBlackTexture = nullptr;
        DefaultFlatNormalTexture = nullptr;
    }

} // namespace Leon
