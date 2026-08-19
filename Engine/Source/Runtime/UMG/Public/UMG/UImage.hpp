#pragma once

#include "UMG/UWidget.hpp"
#include "RHI/FTexture.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Simple textured image widget (UMG-style UImage lite).
     */
    class UImage : public UWidget {
    public:
        UImage(const std::string& InName = "Image");
        ~UImage() override = default;

        void SetBrushFromTexture(const TRef<FTexture2D>& InTexture) { Texture = InTexture; }
        void SetBrushFromPath(const std::string& InVirtualOrFilePath);
        TRef<FTexture2D> GetBrushTexture() const { return Texture; }
        bool HasBrushTexture() const { return Texture != nullptr; }

        void SetBrushUV(const glm::vec2& InMin, const glm::vec2& InMax) {
            UVMin = InMin;
            UVMax = InMax;
        }
        const glm::vec2& GetBrushUVMin() const { return UVMin; }
        const glm::vec2& GetBrushUVMax() const { return UVMax; }

        void SetTintColor(const glm::vec4& InColor) { Tint = InColor; }
        const glm::vec4& GetTintColor() const { return Tint; }

        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        TRef<FTexture2D> Texture;
        glm::vec4 Tint{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec2 UVMin{0.0f, 0.0f};
        glm::vec2 UVMax{1.0f, 1.0f};
        std::string TexturePath;
    };

} // namespace Leon
