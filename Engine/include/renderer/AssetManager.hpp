#pragma once

#include "core/Base.hpp"
#include "renderer/Material.hpp"
#include "renderer/MaterialInstance.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Leon {

    /**
     * @brief Centralized Asset Manager for deduplicating GPU resources (Textures, Shaders, Materials)
     * across scenes, entities, and renderer passes.
     */
    class FAssetManager {
    public:
        static void Init();
        static void Shutdown();

        // Textures
        static TRef<FTexture2D> GetTexture2D(const std::string& InPath);
        static void AddTexture2D(const std::string& InName, const TRef<FTexture2D>& InTexture);
        static bool HasTexture2D(const std::string& InPath);

        // Shaders
        static TRef<FShader> GetShader(const std::string& InPath);
        static void AddShader(const std::string& InName, const TRef<FShader>& InShader);
        static bool HasShader(const std::string& InPath);

        // Materials & Instances
        static TRef<FMaterial> GetMaterial(const std::string& InPath);
        static void AddMaterial(const std::string& InName, const TRef<FMaterial>& InMaterial);
        static bool HasMaterial(const std::string& InPath);

        static TRef<FMaterial> GetDefaultMaterial();
        static TRef<FMaterialInstance> CreateMaterialInstance(const std::string& InMaterialPath);
        static TRef<FMaterialInstance> CreateMaterialInstance(const TRef<FMaterial>& InParent);

        static void Clear();

    private:
        static std::unordered_map<std::string, TRef<FTexture2D>> s_TextureCache;
        static std::unordered_map<std::string, TRef<FShader>> s_ShaderCache;
        static std::unordered_map<std::string, TRef<FMaterial>> s_MaterialCache;
        static TRef<FMaterial> s_DefaultMaterial;
    };

    using AssetManager = FAssetManager;

} // namespace Leon
