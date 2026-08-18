#include "UMG/UCheckBox.hpp"
#include "UMG/FUIRenderer.hpp"

namespace Leon {

    UCheckBox::UCheckBox(const std::string& InName) : UWidget(InName) { Size = {22.0f, 22.0f}; }

    void UCheckBox::SetIsChecked(bool bInChecked) { bIsChecked = bInChecked; }

    void UCheckBox::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;
        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        FUIRenderer::DrawBorderQuad(p0, p1, BoxColor, BorderColor, 1.5f);
        if (bIsChecked) {
            const glm::vec2 inset{4.0f, 4.0f};
            FUIRenderer::DrawQuad(p0 + inset, p1 - inset, CheckColor);
        }
    }

    bool UCheckBox::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsHitTestable() || InButton != 0 || !HitTest(InMousePos))
            return false;
        bIsChecked = !bIsChecked;
        if (OnCheckStateChanged)
            OnCheckStateChanged(bIsChecked);
        return true;
    }

} // namespace Leon
