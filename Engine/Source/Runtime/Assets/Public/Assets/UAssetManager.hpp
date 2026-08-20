#pragma once

#include "Core/Base.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "RHI/FShader.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/FLightmapAsset.hpp"
#include "RHI/FTexture.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Leon {

    /**
     * @brief Centralized Asset Manager for deduplicating and loading GPU resources
     * (Textures, Meshes, Shaders, Materials, Material Instances) across scenes and entities.
     */
    class UAssetManager {
    public:
        static void Init();
        static void Shutdown();

        // Project Content Root Virtual Path Resolution
        static void SetContentRoot(const std::string& InRoot);
        static const std::string& GetContentRoot();
        static std::string ResolveVirtualPath(const std::string& InVirtualPath);

        // Textures (.ltex / standard images)
        static TRef<FTexture2D> GetTexture2D(const std::string& InPath);
        static void AddTexture2D(const std::string& InName, const TRef<FTexture2D>& InTexture);
        static bool HasTexture2D(const std::string& InPath);

        // HDR Environment Textures (.lhdr)
        static TRef<FTexture2D> GetHDRTexture(const std::string& InPath);

        static TRef<FTexture2D> GetDefaultWhiteTexture();
        static TRef<FTexture2D> GetDefaultBlackTexture();
        static TRef<FTexture2D> GetDefaultFlatNormalTexture();
        /** Procedural gray checkerboard (Unreal-style WorldGrid albedo). */
        static TRef<FTexture2D> GetDefaultCheckerTexture();

        // Static Meshes (.lmesh)
        static TRef<UStaticMesh> GetStaticMesh(const std::string& InPath);
        static void AddStaticMesh(const std::string& InName, const TRef<UStaticMesh>& InMesh);
        static bool HasStaticMesh(const std::string& InPath);

        static TRef<USkeleton> GetSkeleton(const std::string& InPath);
        static void AddSkeleton(const std::string& InName, const TRef<USkeleton>& InSkeleton);
        static bool HasSkeleton(const std::string& InPath);

        static TRef<USkeletalMesh> GetSkeletalMesh(const std::string& InPath);
        static void AddSkeletalMesh(const std::string& InName, const TRef<USkeletalMesh>& InMesh);
        static bool HasSkeletalMesh(const std::string& InPath);

        static TRef<UAnimSequence> GetAnimSequence(const std::string& InPath);
        static void AddAnimSequence(const std::string& InName, const TRef<UAnimSequence>& InAnim);
        static bool HasAnimSequence(const std::string& InPath);

        static TRef<UBlendSpace> GetBlendSpace(const std::string& InPath);
        static void AddBlendSpace(const std::string& InName, const TRef<UBlendSpace>& InBlend);
        static bool HasBlendSpace(const std::string& InPath);

        // Lightmaps (.llightmap)
        static TRef<FLightmapAsset> GetLightmap(const std::string& InPath);
        static void AddLightmap(const std::string& InName, const TRef<FLightmapAsset>& InLightmap);
        static bool HasLightmap(const std::string& InPath);
        /** Drop cached .llightmap assets so the next GetLightmap reloads from disk. */
        static void InvalidateLightmaps();

        // Shaders
        static TRef<FShader> GetShader(const std::string& InPath);
        static void AddShader(const std::string& InName, const TRef<FShader>& InShader);
        static bool HasShader(const std::string& InPath);

        // Materials & Instances (.lmat / .lmi)
        static TRef<FMaterial> GetMaterial(const std::string& InPath);
        static void AddMaterial(const std::string& InName, const TRef<FMaterial>& InMaterial);
        static bool HasMaterial(const std::string& InPath);

        static TRef<FMaterialInstance> GetMaterialInstance(const std::string& InPath);
        static void AddMaterialInstance(const std::string& InName, const TRef<FMaterialInstance>& InInstance);
        static bool HasMaterialInstance(const std::string& InPath);

        static TRef<FMaterial> GetDefaultMaterial();
        /** Shared fallback instance. Do not mutate from the renderer. */
        static TRef<FMaterialInstance> GetDefaultMaterialInstance();
        /**
         * Built-in Unreal-like WorldGrid material (gray checker albedo).
         * Virtual path: Engine/Materials/M_WorldGrid.lmat
         */
        static TRef<FMaterial> GetWorldGridMaterial();
        static TRef<FMaterialInstance> GetWorldGridMaterialInstance();
        static TRef<FMaterialInstance> CreateMaterialInstance(const std::string& InMaterialPath);
        static TRef<FMaterialInstance> CreateMaterialInstance(const TRef<FMaterial>& InParent);

        // Generic Loader API
        template <typename T> static TRef<T> Load(const std::string& InPath);

        static void Clear();
        /** Drop cached textures/materials so the next load respects the current max texture resolution. */
        static void ClearLoadedTextures();
        /** Reload all resident material/world textures at the current max texture resolution. */
        static void ReloadAllTextures(class UWorld* InWorld = nullptr);
        /** Drop cache entries whose only remaining owner is the cache itself. Keeps shaders and defaults. */
        static void UnloadUnused();

    private:
        static std::string ContentRoot;
        static std::unordered_map<std::string, TRef<FTexture2D>> TextureCache;
        static std::unordered_map<std::string, TRef<UStaticMesh>> StaticMeshCache;
        static std::unordered_map<std::string, TRef<USkeleton>> SkeletonCache;
        static std::unordered_map<std::string, TRef<USkeletalMesh>> SkeletalMeshCache;
        static std::unordered_map<std::string, TRef<UAnimSequence>> AnimSequenceCache;
        static std::unordered_map<std::string, TRef<UBlendSpace>> BlendSpaceCache;
        static std::unordered_map<std::string, TRef<FLightmapAsset>> LightmapCache;
        static std::unordered_map<std::string, TRef<FShader>> ShaderCache;
        static std::unordered_map<std::string, TRef<FMaterial>> MaterialCache;
        static std::unordered_map<std::string, TRef<FMaterialInstance>> MaterialInstanceCache;
        static TRef<FMaterial> DefaultMaterial;
        static TRef<FMaterialInstance> DefaultMaterialInstance;
        static TRef<FMaterial> WorldGridMaterial;
        static TRef<FTexture2D> DefaultWhiteTexture;
        static TRef<FTexture2D> DefaultBlackTexture;
        static TRef<FTexture2D> DefaultFlatNormalTexture;
        static TRef<FTexture2D> DefaultCheckerTexture;
    };

    template <> inline TRef<FTexture2D> UAssetManager::Load<FTexture2D>(const std::string& InPath) {
        return GetTexture2D(InPath);
    }

    template <> inline TRef<UStaticMesh> UAssetManager::Load<UStaticMesh>(const std::string& InPath) {
        return GetStaticMesh(InPath);
    }

    template <> inline TRef<USkeleton> UAssetManager::Load<USkeleton>(const std::string& InPath) {
        return GetSkeleton(InPath);
    }

    template <> inline TRef<USkeletalMesh> UAssetManager::Load<USkeletalMesh>(const std::string& InPath) {
        return GetSkeletalMesh(InPath);
    }

    template <> inline TRef<UAnimSequence> UAssetManager::Load<UAnimSequence>(const std::string& InPath) {
        return GetAnimSequence(InPath);
    }

    template <> inline TRef<UBlendSpace> UAssetManager::Load<UBlendSpace>(const std::string& InPath) {
        return GetBlendSpace(InPath);
    }

    template <> inline TRef<FMaterial> UAssetManager::Load<FMaterial>(const std::string& InPath) {
        return GetMaterial(InPath);
    }

    template <> inline TRef<FMaterialInstance> UAssetManager::Load<FMaterialInstance>(const std::string& InPath) {
        return GetMaterialInstance(InPath);
    }

    template <> inline TRef<FShader> UAssetManager::Load<FShader>(const std::string& InPath) {
        return GetShader(InPath);
    }

} // namespace Leon
