#include "UMG/UProgressBar.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>

namespace Leon {

    UProgressBar::UProgressBar(const std::string& InName) : UWidget(InName) { Size = {200.0f, 12.0f}; }

    void UProgressBar::SetPercent(float InPercent) { Percent = std::clamp(InPercent, 0.0f, 1.0f); }

    void UProgressBar::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        FUIRenderer::DrawBorderQuad(p0, p1, BackgroundColor, BorderColor, 1.0f);

        const float fillW = InAllottedGeometry.Size.x * Percent;
        if (fillW > 0.5f) {
            constexpr float inset = 1.0f;
            const glm::vec2 f0{p0.x + inset, p0.y + inset};
            const glm::vec2 f1{p0.x + fillW - inset, p1.y - inset};
            if (f1.x > f0.x && f1.y > f0.y)
                FUIRenderer::DrawQuad(f0, f1, FillColor);
        }
    }

} // namespace Leon
