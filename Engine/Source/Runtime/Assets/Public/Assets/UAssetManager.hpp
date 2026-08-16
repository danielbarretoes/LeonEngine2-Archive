#pragma once

#include "Core/Base.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "RHI/FShader.hpp"
#include "Assets/UStaticMesh.hpp"
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

        // Static Meshes (.lmesh)
        static TRef<UStaticMesh> GetStaticMesh(const std::string& InPath);
        static void AddStaticMesh(const std::string& InName, const TRef<UStaticMesh>& InMesh);
        static bool HasStaticMesh(const std::string& InPath);

        // Lightmaps (.llightmap)
        static TRef<FLightmapAsset> GetLightmap(const std::string& InPath);
        static void AddLightmap(const std::string& InName, const TRef<FLightmapAsset>& InLightmap);
        static bool HasLightmap(const std::string& InPath);

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
        static TRef<FMaterialInstance> CreateMaterialInstance(const std::string& InMaterialPath);
        static TRef<FMaterialInstance> CreateMaterialInstance(const TRef<FMaterial>& InParent);

        // Generic Loader API
        template <typename T> static TRef<T> Load(const std::string& InPath);

        static void Clear();

    private:
        static std::string ContentRoot;
        static std::unordered_map<std::string, TRef<FTexture2D>> TextureCache;
        static std::unordered_map<std::string, TRef<UStaticMesh>> StaticMeshCache;
        static std::unordered_map<std::string, TRef<FLightmapAsset>> LightmapCache;
        static std::unordered_map<std::string, TRef<FShader>> ShaderCache;
        static std::unordered_map<std::string, TRef<FMaterial>> MaterialCache;
        static std::unordered_map<std::string, TRef<FMaterialInstance>> MaterialInstanceCache;
        static TRef<FMaterial> DefaultMaterial;
        static TRef<FTexture2D> DefaultWhiteTexture;
        static TRef<FTexture2D> DefaultBlackTexture;
        static TRef<FTexture2D> DefaultFlatNormalTexture;
    };

    template <> inline TRef<FTexture2D> UAssetManager::Load<FTexture2D>(const std::string& InPath) {
        return GetTexture2D(InPath);
    }

    template <> inline TRef<UStaticMesh> UAssetManager::Load<UStaticMesh>(const std::string& InPath) {
        return GetStaticMesh(InPath);
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
