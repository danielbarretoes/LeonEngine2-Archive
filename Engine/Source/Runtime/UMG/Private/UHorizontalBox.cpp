#include "UMG/UHorizontalBox.hpp"

#include <algorithm>

namespace Leon {

    UHorizontalBox::UHorizontalBox(const std::string& InName) : UPanelWidget(InName) {}

    void UHorizontalBox::PerformLayout() {
        float x = 0.0f;
        float h = 0.0f;
        bool bFirst = true;
        for (const auto& child : Children) {
            if (!child || child->GetVisibility() == ESlateVisibility::Collapsed)
                continue;
            if (!bFirst)
                x += SlotPadding;
            bFirst = false;
            child->SetPosition({x, 0.0f});
            x += child->GetSize().x;
            h = std::max(h, child->GetSize().y);
        }
        Size = {x, h};
    }

    void UHorizontalBox::Paint(const FGeometry& InAllottedGeometry) {
        PerformLayout();
        UPanelWidget::Paint(InAllottedGeometry);
    }

} // namespace Leon
