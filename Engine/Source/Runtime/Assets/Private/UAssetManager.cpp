#include "Assets/UAssetManager.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Core/FLog.hpp"
#include "Engine/FMaterialSerializer.hpp"
#include "RHI/IRenderDriver.hpp"

#include "Core/FProjectPaths.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Leon {

    std::string UAssetManager::ContentRoot = "";
    std::unordered_map<std::string, TRef<FTexture2D>> UAssetManager::TextureCache;
    std::unordered_map<std::string, TRef<UStaticMesh>> UAssetManager::StaticMeshCache;
    std::unordered_map<std::string, TRef<USkeleton>> UAssetManager::SkeletonCache;
    std::unordered_map<std::string, TRef<USkeletalMesh>> UAssetManager::SkeletalMeshCache;
    std::unordered_map<std::string, TRef<UAnimSequence>> UAssetManager::AnimSequenceCache;
    std::unordered_map<std::string, TRef<UBlendSpace>> UAssetManager::BlendSpaceCache;
    std::unordered_map<std::string, TRef<FLightmapAsset>> UAssetManager::LightmapCache;
    std::unordered_map<std::string, TRef<FShader>> UAssetManager::ShaderCache;
    std::unordered_map<std::string, TRef<FMaterial>> UAssetManager::MaterialCache;
    std::unordered_map<std::string, TRef<FMaterialInstance>> UAssetManager::MaterialInstanceCache;
    TRef<FMaterial> UAssetManager::DefaultMaterial = nullptr;
    TRef<FMaterialInstance> UAssetManager::DefaultMaterialInstance = nullptr;
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

    TRef<USkeleton> UAssetManager::GetSkeleton(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;
        std::string resolved = ResolveVirtualPath(InPath);
        auto it = SkeletonCache.find(resolved);
        if (it != SkeletonCache.end() && it->second)
            return it->second;
        auto skeleton = USkeleton::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (skeleton->LoadFromFile(resolved)) {
            SkeletonCache[resolved] = skeleton;
            if (resolved != InPath)
                SkeletonCache[InPath] = skeleton;
            return skeleton;
        }
        LE_CORE_WARN("UAssetManager: Failed to load skeleton from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddSkeleton(const std::string& InName, const TRef<USkeleton>& InSkeleton) {
        if (!InName.empty() && InSkeleton)
            SkeletonCache[InName] = InSkeleton;
    }

    bool UAssetManager::HasSkeleton(const std::string& InPath) {
        return SkeletonCache.find(InPath) != SkeletonCache.end();
    }

    TRef<USkeletalMesh> UAssetManager::GetSkeletalMesh(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;
        std::string resolved = ResolveVirtualPath(InPath);
        auto it = SkeletalMeshCache.find(resolved);
        if (it != SkeletalMeshCache.end() && it->second)
            return it->second;
        auto mesh = USkeletalMesh::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (mesh->LoadFromFile(resolved)) {
            if (FRenderDriverRegistry::GetActiveDriver())
                mesh->CreateGPUResources();
            SkeletalMeshCache[resolved] = mesh;
            if (resolved != InPath)
                SkeletalMeshCache[InPath] = mesh;
            return mesh;
        }
        LE_CORE_WARN("UAssetManager: Failed to load skeletal mesh from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddSkeletalMesh(const std::string& InName, const TRef<USkeletalMesh>& InMesh) {
        if (!InName.empty() && InMesh)
            SkeletalMeshCache[InName] = InMesh;
    }

    bool UAssetManager::HasSkeletalMesh(const std::string& InPath) {
        return SkeletalMeshCache.find(InPath) != SkeletalMeshCache.end();
    }

    TRef<UAnimSequence> UAssetManager::GetAnimSequence(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;
        std::string resolved = ResolveVirtualPath(InPath);
        auto it = AnimSequenceCache.find(resolved);
        if (it != AnimSequenceCache.end() && it->second)
            return it->second;
        auto anim = UAnimSequence::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (anim->LoadFromFile(resolved)) {
            const std::string virt = FProjectPaths::MakeVirtualPath(resolved);
            anim->SetAssetPath(!virt.empty() ? virt : InPath);
            AnimSequenceCache[resolved] = anim;
            if (resolved != InPath)
                AnimSequenceCache[InPath] = anim;
            if (!virt.empty() && virt != resolved && virt != InPath)
                AnimSequenceCache[virt] = anim;
            return anim;
        }
        LE_CORE_WARN("UAssetManager: Failed to load animation from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddAnimSequence(const std::string& InName, const TRef<UAnimSequence>& InAnim) {
        if (!InName.empty() && InAnim)
            AnimSequenceCache[InName] = InAnim;
    }

    bool UAssetManager::HasAnimSequence(const std::string& InPath) {
        return AnimSequenceCache.find(InPath) != AnimSequenceCache.end();
    }

    TRef<UBlendSpace> UAssetManager::GetBlendSpace(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;
        std::string resolved = ResolveVirtualPath(InPath);
        auto it = BlendSpaceCache.find(resolved);
        if (it != BlendSpaceCache.end() && it->second)
            return it->second;
        auto blend = UBlendSpace::Create(FAssetPath::GetFileNameWithoutExtension(InPath));
        if (blend->LoadFromFile(resolved)) {
            blend->ResolveSequences();
            BlendSpaceCache[resolved] = blend;
            if (resolved != InPath)
                BlendSpaceCache[InPath] = blend;
            return blend;
        }
        LE_CORE_WARN("UAssetManager: Failed to load blend space from \"{0}\"", InPath);
        return nullptr;
    }

    void UAssetManager::AddBlendSpace(const std::string& InName, const TRef<UBlendSpace>& InBlend) {
        if (!InName.empty() && InBlend)
            BlendSpaceCache[InName] = InBlend;
    }

    bool UAssetManager::HasBlendSpace(const std::string& InPath) {
        return BlendSpaceCache.find(InPath) != BlendSpaceCache.end();
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

        // Each .lmat request gets a unique instance; only the parent FMaterial is cached.
        if (FAssetPath::GetExtension(resolved) == "lmat") {
            auto parentMat = GetMaterial(resolved);
            if (!parentMat)
                parentMat = GetDefaultMaterial();
            return parentMat->CreateInstance();
        }

        // Parse .lmi text file — also unique per call so overrides do not leak across actors.
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

    TRef<FMaterialInstance> UAssetManager::GetDefaultMaterialInstance() {
        if (!DefaultMaterialInstance)
            DefaultMaterialInstance = GetDefaultMaterial()->CreateInstance("M_DefaultPBR_Inst");
        return DefaultMaterialInstance;
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
        SkeletonCache.clear();
        SkeletalMeshCache.clear();
        AnimSequenceCache.clear();
        BlendSpaceCache.clear();
        LightmapCache.clear();
        ShaderCache.clear();
        MaterialCache.clear();
        MaterialInstanceCache.clear();
        DefaultMaterial = nullptr;
        DefaultMaterialInstance = nullptr;
        DefaultWhiteTexture = nullptr;
        DefaultBlackTexture = nullptr;
        DefaultFlatNormalTexture = nullptr;
    }

    void UAssetManager::UnloadUnused() {
        auto dropIfOnlyCached = [](auto& cache) {
            std::unordered_map<const void*, int> aliasCount;
            for (const auto& [key, value] : cache) {
                if (value)
                    aliasCount[value.get()]++;
            }
            for (auto it = cache.begin(); it != cache.end();) {
                if (!it->second) {
                    it = cache.erase(it);
                    continue;
                }
                const int aliases = aliasCount[it->second.get()];
                if (it->second.use_count() <= aliases)
                    it = cache.erase(it);
                else
                    ++it;
            }
        };
        dropIfOnlyCached(TextureCache);
        dropIfOnlyCached(StaticMeshCache);
        dropIfOnlyCached(SkeletonCache);
        dropIfOnlyCached(SkeletalMeshCache);
        dropIfOnlyCached(AnimSequenceCache);
        dropIfOnlyCached(BlendSpaceCache);
        dropIfOnlyCached(LightmapCache);
        dropIfOnlyCached(MaterialCache);
        dropIfOnlyCached(MaterialInstanceCache);
        // Shaders and engine defaults stay resident across travel.
    }

} // namespace Leon
