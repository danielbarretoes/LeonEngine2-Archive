#include "UMG/USlider.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>

namespace Leon {

    USlider::USlider(const std::string& InName) : UWidget(InName) { Size = {200.0f, 22.0f}; }

    void USlider::SetValue(float InValue) { Value = std::clamp(InValue, 0.0f, 1.0f); }

    void USlider::ApplyMouseValue(const glm::vec2& InMousePos) {
        const float width = std::max(CachedGeometry.Size.x, 1.0f);
        const float t = std::clamp((InMousePos.x - CachedGeometry.AbsolutePosition.x) / width, 0.0f, 1.0f);
        if (std::abs(t - Value) < 1e-5f)
            return;
        Value = t;
        if (OnValueChanged)
            OnValueChanged(Value);
    }

    void USlider::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;
        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        const float midY = (p0.y + p1.y) * 0.5f;
        FUIRenderer::DrawQuad({p0.x, midY - 3.0f}, {p1.x, midY + 3.0f}, TrackColor);
        const float fillX = p0.x + InAllottedGeometry.Size.x * Value;
        FUIRenderer::DrawQuad({p0.x, midY - 3.0f}, {fillX, midY + 3.0f}, FillColor);
        const glm::vec2 thumb{fillX, midY};
        FUIRenderer::DrawQuad(thumb - glm::vec2(6.0f, 9.0f), thumb + glm::vec2(6.0f, 9.0f), ThumbColor);
    }

    bool USlider::OnMouseMove(const glm::vec2& InMousePos) {
        if (!IsHitTestable())
            return false;
        if (bDragging) {
            ApplyMouseValue(InMousePos);
            return true;
        }
        return HitTest(InMousePos);
    }

    bool USlider::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsHitTestable() || InButton != 0 || !HitTest(InMousePos))
            return false;
        bDragging = true;
        ApplyMouseValue(InMousePos);
        return true;
    }

    bool USlider::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        if (!bDragging)
            return false;
        (void)InButton;
        ApplyMouseValue(InMousePos);
        bDragging = false;
        return true;
    }

} // namespace Leon
