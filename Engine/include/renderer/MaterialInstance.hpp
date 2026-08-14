#pragma once

#include "core/Base.hpp"
#include "renderer/Material.hpp"
#include "renderer/Shader.hpp"
#include "renderer/Texture.hpp"

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
        explicit FMaterialInstance(const TRef<FMaterial>& InParent,
                                   const std::string& InName = "");
        ~FMaterialInstance() = default;

        static TRef<FMaterialInstance> Create(const TRef<FMaterial>& InParent,
                                              const std::string& InName = "");

        // --- Parent & Identification ---
        TRef<FMaterial> GetParent() const { return m_ParentMaterial; }
        void SetParent(const TRef<FMaterial>& InParent) { m_ParentMaterial = InParent; }

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& InName) { m_Name = InName; }

        TRef<FShader> GetShader() const {
            return m_ParentMaterial ? m_ParentMaterial->GetShader() : nullptr;
        }

        const FMaterialPipelineState& GetPipelineState() const {
            static FMaterialPipelineState s_Default;
            return m_ParentMaterial ? m_ParentMaterial->GetPipelineState() : s_Default;
        }

        // --- Resolved Property Getters (Override with Fallback to Parent) ---
        glm::vec3 GetAlbedoColor() const;
        float GetMetallic() const;
        float GetRoughness() const;
        float GetAO() const;
        glm::vec3 GetEmissiveColor() const;
        float GetEmissiveIntensity() const;
        bool GetUsePlanarReflection() const;

        TRef<FTexture2D> GetTexture(uint32_t InSlot) const;
        bool HasTexture(uint32_t InSlot) const;

        // --- Override Setters ---
        void SetAlbedoColor(const glm::vec3& InColor) { m_AlbedoColorOverride = InColor; }
        void ClearAlbedoColorOverride() { m_AlbedoColorOverride.reset(); }

        void SetMetallic(float InMetallic) { m_MetallicOverride = InMetallic; }
        void ClearMetallicOverride() { m_MetallicOverride.reset(); }

        void SetRoughness(float InRoughness) { m_RoughnessOverride = InRoughness; }
        void ClearRoughnessOverride() { m_RoughnessOverride.reset(); }

        void SetAO(float InAO) { m_AOOverride = InAO; }
        void ClearAOOverride() { m_AOOverride.reset(); }

        void SetEmissiveColor(const glm::vec3& InColor) { m_EmissiveColorOverride = InColor; }
        void ClearEmissiveColorOverride() { m_EmissiveColorOverride.reset(); }

        void SetEmissiveIntensity(float InIntensity) { m_EmissiveIntensityOverride = InIntensity; }
        void ClearEmissiveIntensityOverride() { m_EmissiveIntensityOverride.reset(); }

        void SetUsePlanarReflection(bool bUse) { m_bUsePlanarReflectionOverride = bUse; }
        void ClearUsePlanarReflectionOverride() { m_bUsePlanarReflectionOverride.reset(); }

        void SetTexture(uint32_t InSlot, const TRef<FTexture2D>& InTexture);
        void ClearTextureOverride(uint32_t InSlot);

        // --- GPU State Binding ---
        /**
         * @brief Binds material parameters and textures to the currently active shader.
         */
        void Bind(const TRef<FShader>& InShader) const;

    private:
        TRef<FMaterial> m_ParentMaterial;
        std::string m_Name;

        // Sparse Overrides (nullopt means inherit from parent)
        std::optional<glm::vec3> m_AlbedoColorOverride;
        std::optional<float> m_MetallicOverride;
        std::optional<float> m_RoughnessOverride;
        std::optional<float> m_AOOverride;
        std::optional<glm::vec3> m_EmissiveColorOverride;
        std::optional<float> m_EmissiveIntensityOverride;
        std::optional<bool> m_bUsePlanarReflectionOverride;

        std::unordered_map<uint32_t, TRef<FTexture2D>> m_TextureOverrides;
    };

} // namespace Leon
