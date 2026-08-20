#include "UMG/UScrollBox.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>

namespace Leon {

    UScrollBox::UScrollBox(const std::string& InName) : UPanelWidget(InName) {
        Size = {320.0f, 180.0f};
    }

    void UScrollBox::ClampOffset() {
        const float maxScroll = std::max(0.0f, ContentHeight - Size.y);
        ScrollOffset = std::clamp(ScrollOffset, 0.0f, maxScroll);
    }

    void UScrollBox::SetScrollOffset(float InOffset) {
        ScrollOffset = InOffset;
        ClampOffset();
    }

    void UScrollBox::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;
        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        FUIRenderer::DrawQuad(p0, p1, BackgroundColor);

        if (ContentHeight <= 0.0f) {
            float maxY = 0.0f;
            for (const auto& child : Children) {
                if (!child)
                    continue;
                maxY = std::max(maxY, child->GetPosition().y + child->GetSize().y);
            }
            ContentHeight = maxY;
        }
        ClampOffset();

        for (auto& child : Children) {
            if (!child || !child->IsVisible())
                continue;
            const float y0 = child->GetPosition().y - ScrollOffset;
            const float y1 = y0 + child->GetSize().y;
            if (y1 < 0.0f || y0 > InAllottedGeometry.Size.y)
                continue;
            FGeometry childGeom;
            childGeom.Position = {child->GetPosition().x, y0};
            childGeom.Size = child->GetSize();
            childGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition + childGeom.Position;
            child->Paint(childGeom);
        }
    }

    bool UScrollBox::OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) {
        if (!IsHitTestable() || !HitTest(InMousePos))
            return false;
        SetScrollOffset(ScrollOffset - InWheelDelta * 32.0f);
        return true;
    }

} // namespace Leon
