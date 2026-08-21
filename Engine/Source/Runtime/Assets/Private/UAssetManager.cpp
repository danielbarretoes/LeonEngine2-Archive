#include "Assets/UAssetManager.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FEngineBuiltins.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Core/FLog.hpp"
#include "Engine/FMaterialSerializer.hpp"
#include "Engine/UWorld.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "RHI/IRenderDriver.hpp"

#include "Core/FProjectPaths.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

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
    TRef<FMaterial> UAssetManager::WorldGridMaterial = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultWhiteTexture = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultBlackTexture = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultFlatNormalTexture = nullptr;
    TRef<FTexture2D> UAssetManager::DefaultCheckerTexture = nullptr;

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

        // Engine built-ins (WorldGrid + primitive .lmesh) — virtual Engine/* paths.
        FEngineBuiltins::EnsureAndRegister();
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

    TRef<FTexture2D> UAssetManager::GetDefaultCheckerTexture() {
        if (!DefaultCheckerTexture) {
            constexpr uint32_t kSize = 64;
            constexpr uint32_t kCells = 8;
            constexpr uint32_t kCell = kSize / kCells;
            // Packed little-endian RGBA8 (same convention as other default textures)
            constexpr uint32_t kLight = 0xFFB0B0B0;
            constexpr uint32_t kDark = 0xFF585858;

            DefaultCheckerTexture = FTexture2D::Create(kSize, kSize);
            if (DefaultCheckerTexture) {
                std::vector<uint32_t> pixels(kSize * kSize);
                for (uint32_t y = 0; y < kSize; ++y) {
                    for (uint32_t x = 0; x < kSize; ++x) {
                        const bool light = ((x / kCell) + (y / kCell)) % 2 == 0;
                        pixels[y * kSize + x] = light ? kLight : kDark;
                    }
                }
                DefaultCheckerTexture->SetData(pixels.data(),
                                              static_cast<uint32_t>(pixels.size() * sizeof(uint32_t)));
            }
        }
        return DefaultCheckerTexture;
    }

    TRef<FTexture2D> UAssetManager::GetTexture2D(const std::string& InPath) {
        if (InPath.empty())
            return nullptr;

        if (auto it = TextureCache.find(InPath); it != TextureCache.end() && it->second)
            return it->second;

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

        if (auto it = StaticMeshCache.find(InPath); it != StaticMeshCache.end() && it->second)
            return it->second;

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

    void UAssetManager::InvalidateLightmaps() {
        LightmapCache.clear();
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

        if (auto it = MaterialCache.find(InPath); it != MaterialCache.end() && it->second)
            return it->second;

        // Built-in engine materials (registered by FEngineBuiltins; procedural fallback)
        if (InPath == "Engine/Materials/M_WorldGrid.lmat" || InPath == "/Engine/Materials/M_WorldGrid.lmat" ||
            InPath == "Engine/Materials/M_WorldGrid" || InPath == "/Engine/Materials/M_WorldGrid") {
            return GetWorldGridMaterial();
        }

        std::string resolved = ResolveVirtualPath(InPath);

        auto it = MaterialCache.find(resolved);
        if (it != MaterialCache.end() && it->second) {
            return it->second;
        }

        auto material = MakeRef<FMaterial>(InPath);
        material->SetAssetPath(resolved);
        if (FMaterialSerializer::Deserialize(resolved, *material)) {
            MaterialCache[resolved] = material;
            if (resolved != InPath)
                MaterialCache[InPath] = material;
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
            glm::vec3 albedoOverride(-1.0f);
            bool bHasAlbedo = false;

            auto trimKeyValue = [](const std::string& InLine, const char* Key, std::string& OutValue) -> bool {
                const size_t KeyLen = std::strlen(Key);
                size_t Pos = InLine.find(Key);
                if (Pos == std::string::npos)
                    return false;
                size_t Start = Pos + KeyLen;
                while (Start < InLine.size() &&
                       (InLine[Start] == ' ' || InLine[Start] == '\t' || InLine[Start] == ':'))
                    ++Start;
                if (Start < InLine.size() && InLine[Start] == '"') {
                    size_t End = InLine.find('"', Start + 1);
                    if (End == std::string::npos)
                        return false;
                    OutValue = InLine.substr(Start + 1, End - Start - 1);
                    return true;
                }
                OutValue = InLine.substr(Start);
                while (!OutValue.empty() && (OutValue.back() == '\r' || OutValue.back() == ' ' || OutValue.back() == '\t'))
                    OutValue.pop_back();
                return !OutValue.empty();
            };

            while (std::getline(file, line)) {
                // Skip comments / empty
                size_t NonWs = line.find_first_not_of(" \t\r\n");
                if (NonWs == std::string::npos || line[NonWs] == '#' || line[NonWs] == '/')
                    continue;

                std::string Value;
                if (trimKeyValue(line, "Parent", Value)) {
                    parentPath = Value;
                    continue;
                }
                if (trimKeyValue(line, "Roughness", Value)) {
                    try {
                        roughness = std::stof(Value);
                    } catch (...) {
                    }
                    continue;
                }
                if (trimKeyValue(line, "Metallic", Value)) {
                    try {
                        metallic = std::stof(Value);
                    } catch (...) {
                    }
                    continue;
                }
                if (trimKeyValue(line, "NormalScale", Value)) {
                    try {
                        normalScale = std::stof(Value);
                    } catch (...) {
                    }
                    continue;
                }
                if (trimKeyValue(line, "Albedo", Value) || trimKeyValue(line, "AlbedoColor", Value)) {
                    // Accept "r,g,b" or [r, g, b]
                    for (char& C : Value) {
                        if (C == '[' || C == ']' || C == ',')
                            C = ' ';
                    }
                    std::istringstream Iss(Value);
                    float R = 0, G = 0, B = 0;
                    if (Iss >> R >> G >> B) {
                        albedoOverride = glm::vec3(R, G, B);
                        bHasAlbedo = true;
                    }
                }
            }

            auto parentMat = GetMaterial(parentPath);
            if (!parentMat)
                parentMat = GetDefaultMaterial();
            auto instance = parentMat->CreateInstance();
            if (roughness >= 0.0f)
                instance->SetRoughness(roughness);
            if (metallic >= 0.0f)
                instance->SetMetallic(metallic);
            if (normalScale >= 0.0f)
                instance->SetNormalScale(normalScale);
            if (bHasAlbedo)
                instance->SetAlbedoColor(albedoOverride);
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

    TRef<FMaterial> UAssetManager::GetWorldGridMaterial() {
        if (auto it = MaterialCache.find(FEngineBuiltins::kWorldGridMaterial); it != MaterialCache.end() && it->second) {
            WorldGridMaterial = it->second;
            return WorldGridMaterial;
        }
        if (!WorldGridMaterial) {
            WorldGridMaterial = FMaterial::Create("M_WorldGrid");
            WorldGridMaterial->SetAssetPath(FEngineBuiltins::kWorldGridMaterial);
            WorldGridMaterial->SetAlbedoColor(glm::vec3(1.0f));
            WorldGridMaterial->SetMetallic(0.0f);
            WorldGridMaterial->SetRoughness(0.65f);
            WorldGridMaterial->SetAO(1.0f);
            WorldGridMaterial->SetUVTiling({2.0f, 2.0f});
            WorldGridMaterial->SetTexturePath(0, FEngineBuiltins::kWorldGridTexture);
            WorldGridMaterial->SetUseAlbedoMap(true);
            if (auto checker = GetDefaultCheckerTexture()) {
                WorldGridMaterial->SetAlbedoMap(checker);
                AddTexture2D(FEngineBuiltins::kWorldGridTexture, checker);
            }
            AddMaterial(FEngineBuiltins::kWorldGridMaterial, WorldGridMaterial);
        }
        return WorldGridMaterial;
    }

    TRef<FMaterialInstance> UAssetManager::GetWorldGridMaterialInstance() {
        return GetWorldGridMaterial()->CreateInstance("M_WorldGrid_Inst");
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
        WorldGridMaterial = nullptr;
        DefaultWhiteTexture = nullptr;
        DefaultBlackTexture = nullptr;
        DefaultFlatNormalTexture = nullptr;
        DefaultCheckerTexture = nullptr;
    }

    void UAssetManager::ClearLoadedTextures() {
        TextureCache.clear();
        MaterialCache.clear();
        MaterialInstanceCache.clear();
        LightmapCache.clear();
    }

    void UAssetManager::ReloadAllTextures(UWorld* InWorld) {
        TextureCache.clear();
        LightmapCache.clear();

        for (auto& [_, material] : MaterialCache) {
            if (material)
                material->ReloadTextures();
        }

        if (!InWorld)
            return;

        auto& registry = InWorld->GetRegistry();
        for (auto entity : registry.view<FSkyboxComponent>()) {
            auto& sky = registry.get<FSkyboxComponent>(entity);
            if (!sky.HDREnvironmentMapPath.empty())
                sky.HDREnvironmentMap = GetHDRTexture(sky.HDREnvironmentMapPath);
        }

        for (auto entity : registry.view<FMaterialComponent>()) {
            auto& materialComponent = registry.get<FMaterialComponent>(entity);
            if (!materialComponent.AssetPath.empty()) {
                materialComponent.MaterialInstance = GetMaterialInstance(materialComponent.AssetPath);
                continue;
            }
            if (!materialComponent.MaterialInstance)
                continue;
            if (const TRef<FMaterial> parent = materialComponent.MaterialInstance->GetParent())
                parent->ReloadTextures();
            materialComponent.MaterialInstance->ReloadTextureOverrides();
        }

        if (FWorldRenderer* renderer = InWorld->GetWorldRendererIfInitialized())
            renderer->InvalidateEnvironment();
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
