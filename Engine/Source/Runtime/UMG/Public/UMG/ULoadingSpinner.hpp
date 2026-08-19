#pragma once

#include "UMG/UWidget.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Animated radial dot spinner for loading overlays.
     */
    class ULoadingSpinner : public UWidget {
    public:
        ULoadingSpinner(const std::string& InName = "LoadingSpinner");

        void Tick(float InDeltaTime) override;
        void Paint(const FGeometry& InAllottedGeometry) override;

        void SetSpinnerColor(const glm::vec4& InColor) { SpinnerColor = InColor; }
        const glm::vec4& GetSpinnerColor() const { return SpinnerColor; }
        void SetSegmentCount(int InCount);
        int GetSegmentCount() const { return SegmentCount; }
        void SetSpinSpeed(float InRadiansPerSecond) { SpinSpeed = InRadiansPerSecond; }
        float GetSpinSpeed() const { return SpinSpeed; }
        void SetDotRadius(float InRadius) { DotRadius = InRadius; }
        float GetDotRadius() const { return DotRadius; }
        void SetLabel(const std::string& InLabel) { Label = InLabel; }
        const std::string& GetLabel() const { return Label; }
        void SetLabelFontScale(float InScale) { LabelFontScale = InScale; }
        float GetLabelFontScale() const { return LabelFontScale; }
        float GetPhase() const { return Phase; }

    private:
        float Phase = 0.0f;
        int SegmentCount = 12;
        float SpinSpeed = 5.0f;
        float DotRadius = 4.0f;
        float LabelFontScale = 1.0f;
        glm::vec4 SpinnerColor{0.55f, 0.75f, 1.0f, 1.0f};
        glm::vec4 LabelColor{0.85f, 0.90f, 0.98f, 0.95f};
        std::string Label;
    };

} // namespace Leon
