#pragma once

#include "Core/Base.hpp"
#include "RHI/IRenderAPI.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FTexture.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace Leon {

    class FMaterialInstance;

    enum class EAlphaMode : uint8_t { Opaque = 0, Mask = 1, Blend = 2 };

    /**
     * @brief Pipeline state configuration associated with a material.
     */
    struct FMaterialPipelineState {
        ECullMode CullMode = ECullMode::Back;
        bool bDepthTest = true;
        bool bDepthWrite = true;
        EDepthFunc DepthFunc = EDepthFunc::Less;
        bool bBlend = false;
        EBlendFactor SrcBlend = EBlendFactor::SrcAlpha;
        EBlendFactor DstBlend = EBlendFactor::OneMinusSrcAlpha;
    };

    /**
     * @brief Master Material Definition.
     *
     * Defines shader, pipeline state, PBR parameters, texture maps, UV transformation,
     * alpha modes, and feature flags.
     */
    class FMaterial : public std::enable_shared_from_this<FMaterial> {
    public:
        explicit FMaterial(const std::string& InName = "DefaultMaterial", const TRef<FShader>& InShader = nullptr);
        ~FMaterial() = default;

        static TRef<FMaterial> Create(const std::string& InName = "DefaultMaterial",
                                      const TRef<FShader>& InShader = nullptr);

        /** Creates a lightweight instance inheriting all properties of this material */
        TRef<FMaterialInstance> CreateInstance(const std::string& InInstanceName = "");

        // --- Identification ---
        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        const std::string& GetAssetPath() const { return AssetPath; }
        void SetAssetPath(const std::string& InPath) { AssetPath = InPath; }

        // --- Shader ---
        TRef<FShader> GetShader() const { return Shader; }
        void SetShader(const TRef<FShader>& InShader) { Shader = InShader; }

        // --- Pipeline State ---
        const FMaterialPipelineState& GetPipelineState() const { return PipelineState; }
        FMaterialPipelineState& GetPipelineState() { return PipelineState; }
        void SetPipelineState(const FMaterialPipelineState& InState) { PipelineState = InState; }

        // --- PBR Parameters ---
        const glm::vec3& GetAlbedoColor() const { return AlbedoColor; }
        void SetAlbedoColor(const glm::vec3& InColor) { AlbedoColor = InColor; }

        const glm::vec3& GetBaseColor() const { return AlbedoColor; }
        void SetBaseColor(const glm::vec3& InColor) { AlbedoColor = InColor; }

        float GetMetallic() const { return Metallic; }
        void SetMetallic(float InMetallic) { Metallic = InMetallic; }

        float GetRoughness() const { return Roughness; }
        void SetRoughness(float InRoughness) { Roughness = InRoughness; }

        float GetAO() const { return AO; }
        void SetAO(float InAO) { AO = InAO; }

        float GetNormalScale() const { return NormalScale; }
        void SetNormalScale(float InScale) { NormalScale = InScale; }

        float GetOcclusionStrength() const { return OcclusionStrength; }
        void SetOcclusionStrength(float InStrength) { OcclusionStrength = InStrength; }

        const glm::vec3& GetEmissiveColor() const { return EmissiveColor; }
        void SetEmissiveColor(const glm::vec3& InColor) { EmissiveColor = InColor; }

        float GetEmissiveIntensity() const { return EmissiveIntensity; }
        void SetEmissiveIntensity(float InIntensity) { EmissiveIntensity = InIntensity; }

        float GetEmissiveStrength() const { return EmissiveIntensity; }
        void SetEmissiveStrength(float InStrength) { EmissiveIntensity = InStrength; }

        // --- Alpha & Transparency ---
        EAlphaMode GetAlphaMode() const { return AlphaMode; }
        void SetAlphaMode(EAlphaMode InMode);

        float GetAlphaCutoff() const { return AlphaCutoff; }
        void SetAlphaCutoff(float InCutoff) { AlphaCutoff = InCutoff; }

        bool GetDoubleSided() const { return bDoubleSided; }
        void SetDoubleSided(bool bDouble);

        // --- UV Transformation ---
        const glm::vec2& GetUVTiling() const { return UVTiling; }
        void SetUVTiling(const glm::vec2& InTiling) { UVTiling = InTiling; }

        const glm::vec2& GetUVOffset() const { return UVOffset; }
        void SetUVOffset(const glm::vec2& InOffset) { UVOffset = InOffset; }

        void SetUVTransform(const glm::vec2& InTiling, const glm::vec2& InOffset) {
            UVTiling = InTiling;
            UVOffset = InOffset;
        }

        // --- Texture Maps (Slots 0..5, 9) ---
        TRef<FTexture2D> GetTexture(uint32_t InSlot) const;
        void SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture);

        const std::string& GetTexturePath(uint32_t InSlot) const {
            static std::string Empty = "";
            return (InSlot < 12) ? TexturePaths[InSlot] : Empty;
        }
        void SetTexturePath(uint32_t InSlot, const std::string& InPath) {
            if (InSlot < 12)
                TexturePaths[InSlot] = InPath;
        }

        TRef<FTexture2D> GetAlbedoMap() const { return AlbedoMap; }
        void SetAlbedoMap(const TRef<FTexture2D>& InTex) {
            AlbedoMap = InTex;
            bUseAlbedoMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetNormalMap() const { return NormalMap; }
        void SetNormalMap(const TRef<FTexture2D>& InTex) {
            NormalMap = InTex;
            bUseNormalMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetMetallicMap() const { return MetallicMap; }
        void SetMetallicMap(const TRef<FTexture2D>& InTex) {
            MetallicMap = InTex;
            bUseMetallicMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetRoughnessMap() const { return RoughnessMap; }
        void SetRoughnessMap(const TRef<FTexture2D>& InTex) {
            RoughnessMap = InTex;
            bUseRoughnessMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetAOMap() const { return AOMap; }
        void SetAOMap(const TRef<FTexture2D>& InTex) {
            AOMap = InTex;
            bUseAOMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetEmissiveMap() const { return EmissiveMap; }
        void SetEmissiveMap(const TRef<FTexture2D>& InTex) {
            EmissiveMap = InTex;
            bUseEmissiveMap = (InTex != nullptr);
        }

        // --- Material Feature Flags ---
        bool HasAlbedoMap() const { return bUseAlbedoMap && AlbedoMap != nullptr; }
        bool HasNormalMap() const { return bUseNormalMap && NormalMap != nullptr; }
        bool HasMetallicMap() const { return bUseMetallicMap && MetallicMap != nullptr; }
        bool HasRoughnessMap() const { return bUseRoughnessMap && RoughnessMap != nullptr; }
        bool HasAOMap() const { return bUseAOMap && AOMap != nullptr; }
        bool HasEmissiveMap() const { return bUseEmissiveMap && EmissiveMap != nullptr; }

        void SetUseAlbedoMap(bool bUse) { bUseAlbedoMap = bUse; }
        void SetUseNormalMap(bool bUse) { bUseNormalMap = bUse; }
        void SetUseMetallicMap(bool bUse) { bUseMetallicMap = bUse; }
        void SetUseRoughnessMap(bool bUse) { bUseRoughnessMap = bUse; }
        void SetUseAOMap(bool bUse) { bUseAOMap = bUse; }
        void SetUseEmissiveMap(bool bUse) { bUseEmissiveMap = bUse; }

        bool GetUsePlanarReflection() const { return bUsePlanarReflection; }
        void SetUsePlanarReflection(bool bUse) { bUsePlanarReflection = bUse; }

        /** Reload GPU textures from stored paths (respects current max texture resolution). */
        void ReloadTextures();

    private:
        std::string Name;
        std::string AssetPath;
        TRef<FShader> Shader;

        FMaterialPipelineState PipelineState;

        glm::vec3 AlbedoColor{1.0f, 1.0f, 1.0f};
        float Metallic{0.0f};
        float Roughness{0.5f};
        float AO{1.0f};
        float NormalScale{1.0f};
        float OcclusionStrength{1.0f};
        glm::vec3 EmissiveColor{0.0f, 0.0f, 0.0f};
        float EmissiveIntensity{0.0f};
        float AlphaCutoff{0.5f};
        EAlphaMode AlphaMode{EAlphaMode::Opaque};
        bool bDoubleSided{false};

        glm::vec2 UVTiling{1.0f, 1.0f};
        glm::vec2 UVOffset{0.0f, 0.0f};

        TRef<FTexture2D> AlbedoMap;
        TRef<FTexture2D> NormalMap;
        TRef<FTexture2D> MetallicMap;
        TRef<FTexture2D> RoughnessMap;
        TRef<FTexture2D> AOMap;
        TRef<FTexture2D> EmissiveMap;
        std::string TexturePaths[12];

        bool bUseAlbedoMap{false};
        bool bUseNormalMap{false};
        bool bUseMetallicMap{false};
        bool bUseRoughnessMap{false};
        bool bUseAOMap{false};
        bool bUseEmissiveMap{false};
        bool bUsePlanarReflection{false};
    };

} // namespace Leon
