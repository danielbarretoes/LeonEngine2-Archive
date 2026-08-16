#pragma once

#include "UMG/UWidget.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned text block UI widget.
     */
    class UTextBlock : public UWidget {
    public:
        UTextBlock(const std::string& InName = "TextBlock");
        ~UTextBlock() override = default;

        void SetText(const std::string& InText);
        const std::string& GetText() const { return Text; }

        void SetColor(const glm::vec4& InColor) { Color = InColor; }
        const glm::vec4& GetColor() const { return Color; }

        void SetFontScale(float InScale) {
            FontScale = InScale;
            AutoSize();
        }
        float GetFontScale() const { return FontScale; }

        void SetJustification(ETextAlignment InAlign) { Justification = InAlign; }
        ETextAlignment GetJustification() const { return Justification; }

        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        void AutoSize();

        std::string Text;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float FontScale = 1.0f;
        ETextAlignment Justification = ETextAlignment::Left;
    };

} // namespace Leon
