#pragma once

#include "core/Base.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/Shader.hpp"
#include "renderer/StaticMesh.hpp"
#include "renderer/Texture.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Leon {

    /**
     * @brief Centralized Asset Manager for deduplicating and loading GPU resources
     * (Textures, Meshes, Shaders, Materials, Material Instances) across scenes and entities.
     */
    class FAssetManager {
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
        static TRef<FStaticMesh> GetStaticMesh(const std::string& InPath);
        static void AddStaticMesh(const std::string& InName, const TRef<FStaticMesh>& InMesh);
        static bool HasStaticMesh(const std::string& InPath);

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
        static std::string s_ContentRoot;
        static std::unordered_map<std::string, TRef<FTexture2D>> s_TextureCache;
        static std::unordered_map<std::string, TRef<FStaticMesh>> s_StaticMeshCache;
        static std::unordered_map<std::string, TRef<FShader>> s_ShaderCache;
        static std::unordered_map<std::string, TRef<FMaterial>> s_MaterialCache;
        static std::unordered_map<std::string, TRef<FMaterialInstance>> s_MaterialInstanceCache;
        static TRef<FMaterial> s_DefaultMaterial;
        static TRef<FTexture2D> s_DefaultWhiteTexture;
        static TRef<FTexture2D> s_DefaultBlackTexture;
        static TRef<FTexture2D> s_DefaultFlatNormalTexture;
    };

    template <> inline TRef<FTexture2D> FAssetManager::Load<FTexture2D>(const std::string& InPath) {
        return GetTexture2D(InPath);
    }

    template <> inline TRef<FStaticMesh> FAssetManager::Load<FStaticMesh>(const std::string& InPath) {
        return GetStaticMesh(InPath);
    }

    template <> inline TRef<FMaterial> FAssetManager::Load<FMaterial>(const std::string& InPath) {
        return GetMaterial(InPath);
    }

    template <> inline TRef<FMaterialInstance> FAssetManager::Load<FMaterialInstance>(const std::string& InPath) {
        return GetMaterialInstance(InPath);
    }

    template <> inline TRef<FShader> FAssetManager::Load<FShader>(const std::string& InPath) {
        return GetShader(InPath);
    }

    using AssetManager = FAssetManager;

} // namespace Leon
