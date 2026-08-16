#pragma once

#include "Core/Base.hpp"
#include "Renderer/FMaterial.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FTexture.hpp"

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace Leon {

    /**
     * @brief Lightweight Material Instance with Sparse Parameter & Texture Overrides.
     *
     * Inherits all defaults from its parent FMaterial. Any parameter not explicitly overridden
     * falls back to the parent material, ensuring zero duplicate allocations.
     */
    class FMaterialInstance {
    public:
        explicit FMaterialInstance(const TRef<FMaterial>& InParent, const std::string& InName = "");
        ~FMaterialInstance() = default;

        static TRef<FMaterialInstance> Create(const TRef<FMaterial>& InParent, const std::string& InName = "");

        // --- Parent & Identification ---
        TRef<FMaterial> GetParent() const { return ParentMaterial; }
        void SetParent(const TRef<FMaterial>& InParent) { ParentMaterial = InParent; }

        const std::string& GetName() const { return Name; }
        void SetName(const std::string& InName) { Name = InName; }

        TRef<FShader> GetShader() const { return ParentMaterial ? ParentMaterial->GetShader() : nullptr; }

        const FMaterialPipelineState& GetPipelineState() const {
            static FMaterialPipelineState Default;
            return ParentMaterial ? ParentMaterial->GetPipelineState() : Default;
        }

        // --- Resolved Property Getters (Override with Fallback to Parent) ---
        glm::vec3 GetAlbedoColor() const;
        glm::vec3 GetBaseColor() const { return GetAlbedoColor(); }
        float GetMetallic() const;
        float GetRoughness() const;
        float GetAO() const;
        float GetNormalScale() const;
        float GetOcclusionStrength() const;
        glm::vec3 GetEmissiveColor() const;
        float GetEmissiveIntensity() const;
        float GetEmissiveStrength() const { return GetEmissiveIntensity(); }
        EAlphaMode GetAlphaMode() const;
        float GetAlphaCutoff() const;
        bool GetDoubleSided() const;
        glm::vec2 GetUVTiling() const;
        glm::vec2 GetUVOffset() const;
        bool GetUsePlanarReflection() const;

        TRef<FTexture2D> GetTexture(uint32_t InSlot) const;
        bool HasTexture(uint32_t InSlot) const;

        // --- Override Setters ---
        void SetAlbedoColor(const glm::vec3& InColor) { AlbedoColorOverride = InColor; }
        void SetBaseColor(const glm::vec3& InColor) { AlbedoColorOverride = InColor; }
        void ClearAlbedoColorOverride() { AlbedoColorOverride.reset(); }

        void SetMetallic(float InMetallic) { MetallicOverride = InMetallic; }
        void ClearMetallicOverride() { MetallicOverride.reset(); }

        void SetRoughness(float InRoughness) { RoughnessOverride = InRoughness; }
        void ClearRoughnessOverride() { RoughnessOverride.reset(); }

        void SetAO(float InAO) { AOOverride = InAO; }
        void ClearAOOverride() { AOOverride.reset(); }

        void SetNormalScale(float InScale) { NormalScaleOverride = InScale; }
        void ClearNormalScaleOverride() { NormalScaleOverride.reset(); }

        void SetOcclusionStrength(float InStrength) { OcclusionStrengthOverride = InStrength; }
        void ClearOcclusionStrengthOverride() { OcclusionStrengthOverride.reset(); }

        void SetEmissiveColor(const glm::vec3& InColor) { EmissiveColorOverride = InColor; }
        void ClearEmissiveColorOverride() { EmissiveColorOverride.reset(); }

        void SetEmissiveIntensity(float InIntensity) { EmissiveIntensityOverride = InIntensity; }
        void SetEmissiveStrength(float InStrength) { EmissiveIntensityOverride = InStrength; }
        void ClearEmissiveIntensityOverride() { EmissiveIntensityOverride.reset(); }

        void SetAlphaMode(EAlphaMode InMode) { AlphaModeOverride = InMode; }
        void ClearAlphaModeOverride() { AlphaModeOverride.reset(); }

        void SetAlphaCutoff(float InCutoff) { AlphaCutoffOverride = InCutoff; }
        void ClearAlphaCutoffOverride() { AlphaCutoffOverride.reset(); }

        void SetDoubleSided(bool bDouble) { bDoubleSidedOverride = bDouble; }
        void ClearDoubleSidedOverride() { bDoubleSidedOverride.reset(); }

        void SetUVTiling(const glm::vec2& InTiling) { UVTilingOverride = InTiling; }
        void ClearUVTilingOverride() { UVTilingOverride.reset(); }

        void SetUVOffset(const glm::vec2& InOffset) { UVOffsetOverride = InOffset; }
        void ClearUVOffsetOverride() { UVOffsetOverride.reset(); }

        void SetUVTransform(const glm::vec2& InTiling, const glm::vec2& InOffset) {
            UVTilingOverride = InTiling;
            UVOffsetOverride = InOffset;
        }
        void ClearUVTransformOverride() {
            UVTilingOverride.reset();
            UVOffsetOverride.reset();
        }

        void SetUsePlanarReflection(bool bUse) { bUsePlanarReflectionOverride = bUse; }
        void ClearUsePlanarReflectionOverride() { bUsePlanarReflectionOverride.reset(); }

        void SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture);
        void ClearTextureOverride(uint32_t InSlot);

        // --- GPU State Binding ---
        /**
         * @brief Binds material parameters and textures to the currently active shader.
         */
        void Bind(const TRef<FShader>& InShader) const;

    private:
        TRef<FMaterial> ParentMaterial;
        std::string Name;

        // Sparse Overrides (nullopt means inherit from parent)
        std::optional<glm::vec3> AlbedoColorOverride;
        std::optional<float> MetallicOverride;
        std::optional<float> RoughnessOverride;
        std::optional<float> AOOverride;
        std::optional<float> NormalScaleOverride;
        std::optional<float> OcclusionStrengthOverride;
        std::optional<glm::vec3> EmissiveColorOverride;
        std::optional<float> EmissiveIntensityOverride;
        std::optional<EAlphaMode> AlphaModeOverride;
        std::optional<float> AlphaCutoffOverride;
        std::optional<bool> bDoubleSidedOverride;
        std::optional<glm::vec2> UVTilingOverride;
        std::optional<glm::vec2> UVOffsetOverride;
        std::optional<bool> bUsePlanarReflectionOverride;

        std::unordered_map<uint32_t, TRef<FTexture2D>> TextureOverrides;
    };

} // namespace Leon
