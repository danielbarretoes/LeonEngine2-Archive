#pragma once

#include "UMG/UWidget.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace Leon {

    class UEditableText : public UWidget {
    public:
        UEditableText(const std::string& InName = "EditableText");

        void SetText(const std::string& InText);
        const std::string& GetText() const { return Text; }
        void SetHint(const std::string& InHint) { Hint = InHint; }

        void Tick(float InDeltaTime) override;
        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;

        std::function<void(const std::string&)> OnTextChanged;

    private:
        std::string Text;
        std::string Hint = "127.0.0.1";
        bool bFocused = false;
        bool bBackspaceWasDown = false;
        uint8_t DigitWasDown = 0;
        bool bPeriodWasDown = false;
    };

} // namespace Leon
