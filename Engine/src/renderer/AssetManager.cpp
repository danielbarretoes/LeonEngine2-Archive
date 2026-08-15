#include "renderer/AssetManager.hpp"
#include "asset/AssetPath.hpp"
#include "core/Log.hpp"
#include "world/MaterialSerializer.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Leon {

    std::string FAssetManager::s_ContentRoot = "";
    std::unordered_map<std::string, TRef<FTexture2D>> FAssetManager::s_TextureCache;
    std::unordered_map<std::string, TRef<FStaticMesh>> FAssetManager::s_StaticMeshCache;
    std::unordered_map<std::string, TRef<FShader>> FAssetManager::s_ShaderCache;
    std::unordered_map<std::string, TRef<FMaterial>> FAssetManager::s_MaterialCache;
    std::unordered_map<std::string, TRef<FMaterialInstance>> FAssetManager::s_MaterialInstanceCache;
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

    void FAssetManager::SetContentRoot(const std::string& InRoot) {
        s_ContentRoot = FAssetPath::Normalize(InRoot);
    }

    const std::string& FAssetManager::GetContentRoot() {
        return s_ContentRoot;
    }

    std::string FAssetManager::ResolveVirtualPath(const std::string& InVirtualPath) {
        if (InVirtualPath.empty())
            return "";

        std::string norm = FAssetPath::Normalize(InVirtualPath);

        // 1. If file exists directly as given
        if (std::filesystem::exists(norm))
            return norm;

        // 2. If content root is set, check relative to content root
        if (!s_ContentRoot.empty()) {
            std::string combined = FAssetPath::Combine(s_ContentRoot, norm);
            if (std::filesystem::exists(combined))
                return combined;
        }

        // 3. Standard engine fallback search roots
        std::vector<std::string> searchRoots = {"Projects/Sandbox/Content", "Content"};

        for (const auto& root : searchRoots) {
            std::string candidate = FAssetPath::Combine(root, norm);
            if (std::filesystem::exists(candidate))
                return candidate;
        }

        return norm;
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

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_TextureCache.find(resolved);
        if (it != s_TextureCache.end() && it->second) {
            return it->second;
        }

        TRef<FTexture2D> texture = FTexture2D::Create(resolved);
        if (texture && texture->IsLoaded()) {
            s_TextureCache[resolved] = texture;
            if (resolved != InPath) {
                s_TextureCache[InPath] = texture;
            }
            return texture;
        }

        LE_CORE_WARN("FAssetManager: Failed to load texture from \"{0}\"", InPath);
        return nullptr;
    }

    TRef<FTexture2D> FAssetManager::GetHDRTexture(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_TextureCache.find(resolved);
        if (it != s_TextureCache.end() && it->second) {
            return it->second;
        }

        TRef<FTexture2D> texture = FTexture2D::Create(resolved);
        if (texture && texture->IsLoaded()) {
            s_TextureCache[resolved] = texture;
            if (resolved != InPath) {
                s_TextureCache[InPath] = texture;
            }
            return texture;
        }

        LE_CORE_WARN("FAssetManager: Failed to load HDR texture from \"{0}\"", InPath);
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

    TRef<FStaticMesh> FAssetManager::GetStaticMesh(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_StaticMeshCache.find(resolved);
        if (it != s_StaticMeshCache.end() && it->second) {
            return it->second;
        }

        auto mesh = FStaticMesh::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (mesh->LoadFromFile(resolved)) {
            mesh->CreateGPUResources();
            s_StaticMeshCache[resolved] = mesh;
            if (resolved != InPath) {
                s_StaticMeshCache[InPath] = mesh;
            }
            return mesh;
        }

        LE_CORE_WARN("FAssetManager: Failed to load static mesh from \"{0}\"", InPath);
        return nullptr;
    }

    void FAssetManager::AddStaticMesh(const std::string& InName, const TRef<FStaticMesh>& InMesh) {
        if (!InName.empty() && InMesh) {
            s_StaticMeshCache[InName] = InMesh;
        }
    }

    bool FAssetManager::HasStaticMesh(const std::string& InPath) {
        return s_StaticMeshCache.find(InPath) != s_StaticMeshCache.end();
    }

    TRef<FShader> FAssetManager::GetShader(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_ShaderCache.find(resolved);
        if (it != s_ShaderCache.end() && it->second) {
            return it->second;
        }

        TRef<FShader> shader = FShader::Create(resolved);
        if (shader) {
            s_ShaderCache[resolved] = shader;
            if (resolved != InPath) {
                s_ShaderCache[InPath] = shader;
            }
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

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_MaterialCache.find(resolved);
        if (it != s_MaterialCache.end() && it->second) {
            return it->second;
        }

        auto material = MakeRef<FMaterial>(InPath);
        material->SetAssetPath(resolved);
        if (FMaterialSerializer::Deserialize(resolved, *material)) {
            s_MaterialCache[resolved] = material;
            if (resolved != InPath) {
                s_MaterialCache[InPath] = material;
            }
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

    TRef<FMaterialInstance> FAssetManager::GetMaterialInstance(const std::string& InPath) {
        if (InPath.empty())
            return GetDefaultMaterial()->CreateInstance();

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = s_MaterialInstanceCache.find(resolved);
        if (it != s_MaterialInstanceCache.end() && it->second) {
            return it->second;
        }

        // If path ends with .lmat, load parent material and create instance
        if (FAssetPath::GetExtension(resolved) == "lmat") {
            auto parentMat = GetMaterial(resolved);
            auto instance = parentMat->CreateInstance();
            s_MaterialInstanceCache[resolved] = instance;
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

            s_MaterialInstanceCache[resolved] = instance;
            return instance;
        }

        return GetDefaultMaterial()->CreateInstance();
    }

    void FAssetManager::AddMaterialInstance(const std::string& InName, const TRef<FMaterialInstance>& InInstance) {
        if (!InName.empty() && InInstance) {
            s_MaterialInstanceCache[InName] = InInstance;
        }
    }

    bool FAssetManager::HasMaterialInstance(const std::string& InPath) {
        return s_MaterialInstanceCache.find(InPath) != s_MaterialInstanceCache.end();
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
        s_StaticMeshCache.clear();
        s_ShaderCache.clear();
        s_MaterialCache.clear();
        s_MaterialInstanceCache.clear();
        s_DefaultMaterial = nullptr;
        s_DefaultWhiteTexture = nullptr;
        s_DefaultBlackTexture = nullptr;
        s_DefaultFlatNormalTexture = nullptr;
    }

} // namespace Leon
