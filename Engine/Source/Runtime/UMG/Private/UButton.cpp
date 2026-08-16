#include "UMG/UButton.hpp"
#include "UMG/FUIRenderer.hpp"

namespace Leon {

    UButton::UButton(const std::string& InName) : UPanelWidget(InName) {
        Size = glm::vec2(160.0f, 40.0f);
    }

    EButtonState UButton::GetCurrentState() const {
        if (!bIsEnabled)
            return EButtonState::Disabled;
        if (bIsPressed)
            return EButtonState::Pressed;
        if (bIsHovered)
            return EButtonState::Hovered;
        return EButtonState::Normal;
    }

    void UButton::SetContent(const TRef<UWidget>& InContent) {
        ClearChildren();
        if (InContent) {
            AddChild(InContent);
        }
    }

    TRef<UWidget> UButton::GetContent() const {
        if (!Children.empty())
            return Children.front();
        return nullptr;
    }

    void UButton::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        // Choose background fill color based on state
        glm::vec4 fillColor = NormalColor;
        glm::vec4 borderColor = BorderColor;

        switch (GetCurrentState()) {
        case EButtonState::Normal:
            fillColor = NormalColor;
            break;
        case EButtonState::Hovered:
            fillColor = HoveredColor;
            borderColor = glm::vec4(0.5f, 0.75f, 1.0f, 1.0f);
            break;
        case EButtonState::Pressed:
            fillColor = PressedColor;
            borderColor = glm::vec4(0.8f, 0.9f, 1.0f, 1.0f);
            break;
        case EButtonState::Disabled:
            fillColor = DisabledColor;
            borderColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.4f);
            break;
        }

        glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        FUIRenderer::DrawBorderQuad(p0, p1, fillColor, borderColor, BorderWidth);

        // Center content inside the button
        for (auto& child : Children) {
            if (!child || !child->IsVisible())
                continue;

            FGeometry childGeom;
            childGeom.Size = child->GetSize();
            // Center alignment
            glm::vec2 offset = (InAllottedGeometry.Size - child->GetSize()) * 0.5f;
            if (offset.x < 0.0f)
                offset.x = 0.0f;
            if (offset.y < 0.0f)
                offset.y = 0.0f;

            childGeom.Position = offset;
            childGeom.AbsolutePosition = InAllottedGeometry.AbsolutePosition + offset;
            child->Paint(childGeom);
        }
    }

    bool UButton::OnMouseMove(const glm::vec2& InMousePos) {
        if (!IsVisible() || !bIsEnabled) {
            bIsHovered = false;
            return false;
        }

        bIsHovered = CachedGeometry.IsUnderLocation(InMousePos);
        return bIsHovered;
    }

    bool UButton::OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible() || !bIsEnabled)
            return false;

        if (InButton == 0 && CachedGeometry.IsUnderLocation(InMousePos)) {
            bIsPressed = true;
            return true;
        }
        return false;
    }

    bool UButton::OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) {
        if (!IsVisible() || !bIsEnabled) {
            bIsPressed = false;
            return false;
        }

        if (InButton == 0 && bIsPressed) {
            bIsPressed = false;
            if (CachedGeometry.IsUnderLocation(InMousePos)) {
                OnClicked.Broadcast();
                return true;
            }
        }
        bIsPressed = false;
        return false;
    }

} // namespace Leon
