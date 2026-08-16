#include "UMG/UPanelWidget.hpp"
#include <algorithm>

namespace Leon {

    UPanelWidget::UPanelWidget(const std::string& InName) : UWidget(InName) {}

    void UPanelWidget::AddChild(const TRef<UWidget>& InChild) {
        if (!InChild)
            return;
        InChild->SetParent(this);
        Children.push_back(InChild);
    }

    bool UPanelWidget::RemoveChild(const TRef<UWidget>& InChild) {
        auto it = std::find(Children.begin(), Children.end(), InChild);
        if (it != Children.end()) {
            (*it)->SetParent(nullptr);
            Children.erase(it);
            return true;
        }
        return false;
    }

    void UPanelWidget::ClearChildren() {
        for (auto& child : Children) {
            if (child)
                child->SetParent(nullptr);
        }
        Children.clear();
    }

    void UPanelWidget::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        for (auto& child : Children) {
            if (!child || !child->IsVisible())
                continue;

            FGeometry childGeom;
            childGeom.Position = child->GetPosition();
            childGeom.Size = child->GetSize();
            childGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition + child->GetPosition();

            child->Paint(childGeom);
        }
    }

    void UPanelWidget::Tick(float InDeltaTime) {
        UWidget::Tick(InDeltaTime);
        for (auto& child : Children) {
            if (child)
                child->Tick(InDeltaTime);
        }
    }

    bool UPanelWidget::OnMouseMove(const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        bool bHandled = false;
        // Iterate in reverse for top-most child first
        for (auto it = Children.rbegin(); it != Children.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseMove(InMousePos)) {
                    bHandled = true;
                }
            }
        }
        return bHandled;
    }

    bool UPanelWidget::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        for (auto it = Children.rbegin(); it != Children.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseButtonDown(InButton, InMousePos)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool UPanelWidget::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible())
            return false;
        for (auto it = Children.rbegin(); it != Children.rend(); ++it) {
            if (*it && (*it)->IsHitTestable()) {
                if ((*it)->OnMouseButtonUp(InButton, InMousePos)) {
                    return true;
                }
            }
        }
        return false;
    }

} // namespace Leon
