#include "UMG/UWidget.hpp"
#include "UMG/UPanelWidget.hpp"

namespace Leon {

    UWidget::UWidget(const std::string& InName) : UObject(InName) {}

    void UWidget::Paint(const FGeometry& InAllottedGeometry) {
        CachedGeometry = InAllottedGeometry;
    }

    void UWidget::RemoveFromParent() {
        if (Parent) {
            // Find shared pointer in parent
            const auto& children = Parent->GetAllChildren();
            for (const auto& child : children) {
                if (child.get() == this) {
                    Parent->RemoveChild(child);
                    break;
                }
            }
            Parent = nullptr;
        }
    }

    bool UWidget::HitTest(const glm::vec2& InPoint) const {
        if (!IsHitTestable())
            return false;
        return CachedGeometry.IsUnderLocation(InPoint);
    }

} // namespace Leon
