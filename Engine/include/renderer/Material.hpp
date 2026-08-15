#pragma once

#include "core/Base.hpp"
#include "renderer/RenderAPI.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"

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
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& InName) { m_Name = InName; }

        const std::string& GetAssetPath() const { return m_AssetPath; }
        void SetAssetPath(const std::string& InPath) { m_AssetPath = InPath; }

        // --- Shader ---
        TRef<FShader> GetShader() const { return m_Shader; }
        void SetShader(const TRef<FShader>& InShader) { m_Shader = InShader; }

        // --- Pipeline State ---
        const FMaterialPipelineState& GetPipelineState() const { return m_PipelineState; }
        FMaterialPipelineState& GetPipelineState() { return m_PipelineState; }
        void SetPipelineState(const FMaterialPipelineState& InState) { m_PipelineState = InState; }

        // --- PBR Parameters ---
        const glm::vec3& GetAlbedoColor() const { return m_AlbedoColor; }
        void SetAlbedoColor(const glm::vec3& InColor) { m_AlbedoColor = InColor; }

        const glm::vec3& GetBaseColor() const { return m_AlbedoColor; }
        void SetBaseColor(const glm::vec3& InColor) { m_AlbedoColor = InColor; }

        float GetMetallic() const { return m_Metallic; }
        void SetMetallic(float InMetallic) { m_Metallic = InMetallic; }

        float GetRoughness() const { return m_Roughness; }
        void SetRoughness(float InRoughness) { m_Roughness = InRoughness; }

        float GetAO() const { return m_AO; }
        void SetAO(float InAO) { m_AO = InAO; }

        float GetNormalScale() const { return m_NormalScale; }
        void SetNormalScale(float InScale) { m_NormalScale = InScale; }

        float GetOcclusionStrength() const { return m_OcclusionStrength; }
        void SetOcclusionStrength(float InStrength) { m_OcclusionStrength = InStrength; }

        const glm::vec3& GetEmissiveColor() const { return m_EmissiveColor; }
        void SetEmissiveColor(const glm::vec3& InColor) { m_EmissiveColor = InColor; }

        float GetEmissiveIntensity() const { return m_EmissiveIntensity; }
        void SetEmissiveIntensity(float InIntensity) { m_EmissiveIntensity = InIntensity; }

        float GetEmissiveStrength() const { return m_EmissiveIntensity; }
        void SetEmissiveStrength(float InStrength) { m_EmissiveIntensity = InStrength; }

        // --- Alpha & Transparency ---
        EAlphaMode GetAlphaMode() const { return m_AlphaMode; }
        void SetAlphaMode(EAlphaMode InMode);

        float GetAlphaCutoff() const { return m_AlphaCutoff; }
        void SetAlphaCutoff(float InCutoff) { m_AlphaCutoff = InCutoff; }

        bool GetDoubleSided() const { return m_bDoubleSided; }
        void SetDoubleSided(bool bDouble);

        // --- UV Transformation ---
        const glm::vec2& GetUVTiling() const { return m_UVTiling; }
        void SetUVTiling(const glm::vec2& InTiling) { m_UVTiling = InTiling; }

        const glm::vec2& GetUVOffset() const { return m_UVOffset; }
        void SetUVOffset(const glm::vec2& InOffset) { m_UVOffset = InOffset; }

        void SetUVTransform(const glm::vec2& InTiling, const glm::vec2& InOffset) {
            m_UVTiling = InTiling;
            m_UVOffset = InOffset;
        }

        // --- Texture Maps (Slots 0..5, 9) ---
        TRef<FTexture2D> GetTexture(uint32_t InSlot) const;
        void SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture);

        const std::string& GetTexturePath(uint32_t InSlot) const {
            static std::string s_Empty = "";
            return (InSlot < 12) ? m_TexturePaths[InSlot] : s_Empty;
        }
        void SetTexturePath(uint32_t InSlot, const std::string& InPath) {
            if (InSlot < 12)
                m_TexturePaths[InSlot] = InPath;
        }

        TRef<FTexture2D> GetAlbedoMap() const { return m_AlbedoMap; }
        void SetAlbedoMap(const TRef<FTexture2D>& InTex) {
            m_AlbedoMap = InTex;
            m_bUseAlbedoMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetNormalMap() const { return m_NormalMap; }
        void SetNormalMap(const TRef<FTexture2D>& InTex) {
            m_NormalMap = InTex;
            m_bUseNormalMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetMetallicMap() const { return m_MetallicMap; }
        void SetMetallicMap(const TRef<FTexture2D>& InTex) {
            m_MetallicMap = InTex;
            m_bUseMetallicMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetRoughnessMap() const { return m_RoughnessMap; }
        void SetRoughnessMap(const TRef<FTexture2D>& InTex) {
            m_RoughnessMap = InTex;
            m_bUseRoughnessMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetAOMap() const { return m_AOMap; }
        void SetAOMap(const TRef<FTexture2D>& InTex) {
            m_AOMap = InTex;
            m_bUseAOMap = (InTex != nullptr);
        }

        TRef<FTexture2D> GetEmissiveMap() const { return m_EmissiveMap; }
        void SetEmissiveMap(const TRef<FTexture2D>& InTex) {
            m_EmissiveMap = InTex;
            m_bUseEmissiveMap = (InTex != nullptr);
        }

        // --- Material Feature Flags ---
        bool HasAlbedoMap() const { return m_bUseAlbedoMap && m_AlbedoMap != nullptr; }
        bool HasNormalMap() const { return m_bUseNormalMap && m_NormalMap != nullptr; }
        bool HasMetallicMap() const { return m_bUseMetallicMap && m_MetallicMap != nullptr; }
        bool HasRoughnessMap() const { return m_bUseRoughnessMap && m_RoughnessMap != nullptr; }
        bool HasAOMap() const { return m_bUseAOMap && m_AOMap != nullptr; }
        bool HasEmissiveMap() const { return m_bUseEmissiveMap && m_EmissiveMap != nullptr; }

        void SetUseAlbedoMap(bool bUse) { m_bUseAlbedoMap = bUse; }
        void SetUseNormalMap(bool bUse) { m_bUseNormalMap = bUse; }
        void SetUseMetallicMap(bool bUse) { m_bUseMetallicMap = bUse; }
        void SetUseRoughnessMap(bool bUse) { m_bUseRoughnessMap = bUse; }
        void SetUseAOMap(bool bUse) { m_bUseAOMap = bUse; }
        void SetUseEmissiveMap(bool bUse) { m_bUseEmissiveMap = bUse; }

        bool GetUsePlanarReflection() const { return m_bUsePlanarReflection; }
        void SetUsePlanarReflection(bool bUse) { m_bUsePlanarReflection = bUse; }

    private:
        std::string m_Name;
        std::string m_AssetPath;
        TRef<FShader> m_Shader;

        FMaterialPipelineState m_PipelineState;

        glm::vec3 m_AlbedoColor{1.0f, 1.0f, 1.0f};
        float m_Metallic{0.0f};
        float m_Roughness{0.5f};
        float m_AO{1.0f};
        float m_NormalScale{1.0f};
        float m_OcclusionStrength{1.0f};
        glm::vec3 m_EmissiveColor{0.0f, 0.0f, 0.0f};
        float m_EmissiveIntensity{0.0f};
        float m_AlphaCutoff{0.5f};
        EAlphaMode m_AlphaMode{EAlphaMode::Opaque};
        bool m_bDoubleSided{false};

        glm::vec2 m_UVTiling{1.0f, 1.0f};
        glm::vec2 m_UVOffset{0.0f, 0.0f};

        TRef<FTexture2D> m_AlbedoMap;
        TRef<FTexture2D> m_NormalMap;
        TRef<FTexture2D> m_MetallicMap;
        TRef<FTexture2D> m_RoughnessMap;
        TRef<FTexture2D> m_AOMap;
        TRef<FTexture2D> m_EmissiveMap;
        std::string m_TexturePaths[12];

        bool m_bUseAlbedoMap{false};
        bool m_bUseNormalMap{false};
        bool m_bUseMetallicMap{false};
        bool m_bUseRoughnessMap{false};
        bool m_bUseAOMap{false};
        bool m_bUseEmissiveMap{false};
        bool m_bUsePlanarReflection{false};
    };

} // namespace Leon
