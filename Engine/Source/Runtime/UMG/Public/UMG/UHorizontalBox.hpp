#pragma once

#include "UMG/UPanelWidget.hpp"
#include <algorithm>

namespace Leon {

    /** Left-to-right stack of children using each child's Size. */
    class UHorizontalBox : public UPanelWidget {
    public:
        UHorizontalBox(const std::string& InName = "HorizontalBox");

        void SetSlotPadding(float InPadding) { SlotPadding = std::max(0.0f, InPadding); }
        float GetSlotPadding() const { return SlotPadding; }

        void PerformLayout();
        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        float SlotPadding = 4.0f;
    };

} // namespace Leon
