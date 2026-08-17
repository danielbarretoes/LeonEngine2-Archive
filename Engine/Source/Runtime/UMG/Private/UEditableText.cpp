#include "UMG/UEditableText.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Core/FInput.hpp"

namespace Leon {

    UEditableText::UEditableText(const std::string& InName) : UWidget(InName) {
        Size = {280.0f, 36.0f};
    }

    void UEditableText::SetText(const std::string& InText) {
        Text = InText;
        if (OnTextChanged)
            OnTextChanged(Text);
    }

    bool UEditableText::OnMouseButtonDown(int, const glm::vec2& InMousePos) {
        bFocused = HitTest(InMousePos);
        return bFocused;
    }

    void UEditableText::Tick(float) {
        if (!bFocused)
            return;

        const bool bBack = FInput::IsKeyPressed(Key::Backspace);
        if (bBack && !bBackspaceWasDown && !Text.empty()) {
            Text.pop_back();
            if (OnTextChanged)
                OnTextChanged(Text);
        }
        bBackspaceWasDown = bBack;

        const bool bPeriod = FInput::IsKeyPressed(Key::Period);
        if (bPeriod && !bPeriodWasDown && Text.size() < 32) {
            Text.push_back('.');
            if (OnTextChanged)
                OnTextChanged(Text);
        }
        bPeriodWasDown = bPeriod;

        for (int i = 0; i < 10; ++i) {
            const bool down = FInput::IsKeyPressed(Key::D0 + i);
            const uint8_t bit = static_cast<uint8_t>(1u << i);
            if (down && (DigitWasDown & bit) == 0 && Text.size() < 32) {
                Text.push_back(static_cast<char>('0' + i));
                if (OnTextChanged)
                    OnTextChanged(Text);
            }
            if (down)
                DigitWasDown |= bit;
            else
                DigitWasDown &= static_cast<uint8_t>(~bit);
        }
    }

    void UEditableText::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;
        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        const glm::vec4 fill = bFocused ? glm::vec4(0.10f, 0.14f, 0.22f, 0.95f) : glm::vec4(0.06f, 0.08f, 0.12f, 0.90f);
        FUIRenderer::DrawBorderQuad(p0, p1, fill, glm::vec4(0.35f, 0.55f, 0.85f, 0.8f), 1.5f);
        const std::string& shown = Text.empty() ? Hint : Text;
        const glm::vec4 color =
            Text.empty() ? glm::vec4(0.45f, 0.50f, 0.58f, 1.0f) : glm::vec4(0.95f, 0.97f, 1.0f, 1.0f);
        FUIRenderer::DrawString(p0.x + 8.0f, p0.y + 8.0f, shown, color, 0.85f, ETextAlignment::Left);
    }

} // namespace Leon
