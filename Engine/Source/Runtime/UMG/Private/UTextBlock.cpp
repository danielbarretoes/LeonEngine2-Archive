#include "UMG/UTextBlock.hpp"
#include "UMG/FUIRenderer.hpp"

namespace Leon {

    UTextBlock::UTextBlock(const std::string& InName) : UWidget(InName) {}

    void UTextBlock::SetText(const std::string& InText) {
        Text = InText;
        AutoSize();
    }

    void UTextBlock::AutoSize() {
        glm::vec2 measured = FUIRenderer::MeasureString(Text, FontScale);
        if (measured.x > 0.0f && measured.y > 0.0f) {
            Size = measured;
        }
    }

    void UTextBlock::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible() || Text.empty())
            return;

        float textX = InAllottedGeometry.AbsolutePosition.x;
        if (Justification == ETextAlignment::Center) {
            textX += InAllottedGeometry.Size.x * 0.5f;
        } else if (Justification == ETextAlignment::Right) {
            textX += InAllottedGeometry.Size.x;
        }

        float textY = InAllottedGeometry.AbsolutePosition.y;
        FUIRenderer::DrawString(textX, textY, Text, Color, FontScale, Justification);
    }

} // namespace Leon
