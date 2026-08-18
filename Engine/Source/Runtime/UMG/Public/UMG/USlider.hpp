#pragma once

#include "UMG/UWidget.hpp"

#include <functional>

namespace Leon {

    /**
     * Horizontal value slider in [0, 1]. Drag the thumb or click the track.
     */
    class USlider : public UWidget {
    public:
        USlider(const std::string& InName = "Slider");

        void SetValue(float InValue);
        float GetValue() const { return Value; }

        using FValueChanged = std::function<void(float InValue)>;
        FValueChanged OnValueChanged;

        void Paint(const FGeometry& InAllottedGeometry) override;
        bool OnMouseMove(const glm::vec2& InMousePos) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) override;

    private:
        void ApplyMouseValue(const glm::vec2& InMousePos);
        float Value = 0.0f;
        bool bDragging = false;
        glm::vec4 TrackColor{0.12f, 0.14f, 0.18f, 0.95f};
        glm::vec4 FillColor{0.30f, 0.62f, 0.95f, 0.95f};
        glm::vec4 ThumbColor{0.92f, 0.94f, 0.98f, 1.0f};
    };

} // namespace Leon
