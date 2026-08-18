#pragma once

#include "UMG/UPanelWidget.hpp"

namespace Leon {

    /**
     * Vertically scrolling panel. Children are offset by ScrollOffset.
     */
    class UScrollBox : public UPanelWidget {
    public:
        UScrollBox(const std::string& InName = "ScrollBox");

        void SetScrollOffset(float InOffset);
        float GetScrollOffset() const { return ScrollOffset; }
        void SetContentHeight(float InHeight) { ContentHeight = InHeight; }
        float GetContentHeight() const { return ContentHeight; }

        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) override;

    private:
        void ClampOffset();
        float ScrollOffset = 0.0f;
        float ContentHeight = 0.0f;
        glm::vec4 BackgroundColor{0.05f, 0.06f, 0.08f, 0.40f};
    };

} // namespace Leon
