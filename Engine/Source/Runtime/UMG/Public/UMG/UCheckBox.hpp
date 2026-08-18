#pragma once

#include "UMG/UWidget.hpp"

#include <functional>

namespace Leon {

    /**
     * Toggle box with a checked glyph.
     */
    class UCheckBox : public UWidget {
    public:
        UCheckBox(const std::string& InName = "CheckBox");

        void SetIsChecked(bool bInChecked);
        bool IsChecked() const { return bIsChecked; }

        using FCheckStateChanged = std::function<void(bool bIsChecked)>;
        FCheckStateChanged OnCheckStateChanged;

        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;

    private:
        bool bIsChecked = false;
        glm::vec4 BoxColor{0.10f, 0.12f, 0.16f, 0.95f};
        glm::vec4 BorderColor{0.45f, 0.52f, 0.62f, 0.90f};
        glm::vec4 CheckColor{0.35f, 0.85f, 0.55f, 1.0f};
    };

} // namespace Leon
