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

        void SetTintColor(const glm::vec4& InColor) { Tint = InColor; }
        const glm::vec4& GetTintColor() const { return Tint; }

        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        TRef<FTexture2D> Texture;
        glm::vec4 Tint{1.0f, 1.0f, 1.0f, 1.0f};
        std::string TexturePath;
    };

} // namespace Leon
