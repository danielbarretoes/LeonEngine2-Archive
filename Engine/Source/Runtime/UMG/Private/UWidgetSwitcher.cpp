#include "UMG/UWidgetSwitcher.hpp"

namespace Leon {

    UWidgetSwitcher::UWidgetSwitcher(const std::string& InName) : UPanelWidget(InName) {}

    void UWidgetSwitcher::SetActiveWidgetIndex(int32_t InIndex) {
        if (Children.empty()) {
            ActiveIndex = 0;
            return;
        }
        const int32_t maxIndex = static_cast<int32_t>(Children.size()) - 1;
        ActiveIndex = InIndex < 0 ? 0 : (InIndex > maxIndex ? maxIndex : InIndex);
    }

    UWidget* UWidgetSwitcher::GetActiveWidget() const {
        if (ActiveIndex < 0 || ActiveIndex >= static_cast<int32_t>(Children.size()))
            return nullptr;
        return Children[static_cast<size_t>(ActiveIndex)].get();
    }

    void UWidgetSwitcher::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;
        UWidget* active = GetActiveWidget();
        if (!active || !active->IsVisible())
            return;
        FGeometry childGeom;
        childGeom.Position = active->GetPosition();
        childGeom.Size = active->GetSize();
        childGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition + active->GetPosition();
        active->Paint(childGeom);
    }

    bool UWidgetSwitcher::OnMouseMove(const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        UWidget* active = GetActiveWidget();
        if (active && active->IsHitTestable())
            return active->OnMouseMove(InMousePos);
        return false;
    }

    bool UWidgetSwitcher::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        UWidget* active = GetActiveWidget();
        if (active && active->IsHitTestable())
            return active->OnMouseButtonDown(InButton, InMousePos);
        return false;
    }

    bool UWidgetSwitcher::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        UWidget* active = GetActiveWidget();
        if (active && active->IsHitTestable())
            return active->OnMouseButtonUp(InButton, InMousePos);
        return false;
    }

    bool UWidgetSwitcher::OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        UWidget* active = GetActiveWidget();
        if (active && active->IsHitTestable())
            return active->OnMouseWheel(InWheelDelta, InMousePos);
        return false;
    }

} // namespace Leon
