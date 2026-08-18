#include "UMG/UVerticalBox.hpp"

#include <algorithm>

namespace Leon {

    UVerticalBox::UVerticalBox(const std::string& InName) : UPanelWidget(InName) {}

    void UVerticalBox::PerformLayout() {
        float y = 0.0f;
        float w = 0.0f;
        bool bFirst = true;
        for (const auto& child : Children) {
            if (!child || child->GetVisibility() == ESlateVisibility::Collapsed)
                continue;
            if (!bFirst)
                y += SlotPadding;
            bFirst = false;
            child->SetPosition({0.0f, y});
            y += child->GetSize().y;
            w = std::max(w, child->GetSize().x);
        }
        Size = {w, y};
    }

    void UVerticalBox::Paint(const FGeometry& InAllottedGeometry) {
        PerformLayout();
        UPanelWidget::Paint(InAllottedGeometry);
    }

} // namespace Leon
