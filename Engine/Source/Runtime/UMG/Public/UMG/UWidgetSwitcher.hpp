#pragma once

#include "UMG/UPanelWidget.hpp"

namespace Leon {

    /**
     * Shows exactly one child at a time (Unreal UWidgetSwitcher lite).
     */
    class UWidgetSwitcher : public UPanelWidget {
    public:
        UWidgetSwitcher(const std::string& InName = "WidgetSwitcher");

        void SetActiveWidgetIndex(int32_t InIndex);
        int32_t GetActiveWidgetIndex() const { return ActiveIndex; }
        UWidget* GetActiveWidget() const;

        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseMove(const glm::vec2& InMousePos) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseWheel(float InWheelDelta, const glm::vec2& InMousePos) override;

    private:
        int32_t ActiveIndex = 0;
    };

} // namespace Leon
