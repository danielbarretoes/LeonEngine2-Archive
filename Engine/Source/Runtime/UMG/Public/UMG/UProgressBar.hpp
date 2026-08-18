#pragma once

#include "UMG/UWidget.hpp"

namespace Leon {

    /**
     * @brief Horizontal fill bar (Unreal UProgressBar lite). Percent is [0, 1].
     */
    class UProgressBar : public UWidget {
    public:
        UProgressBar(const std::string& InName = "ProgressBar");

        void SetPercent(float InPercent);
        float GetPercent() const { return Percent; }

        void SetFillColor(const glm::vec4& InColor) { FillColor = InColor; }
        const glm::vec4& GetFillColor() const { return FillColor; }
        void SetBackgroundColor(const glm::vec4& InColor) { BackgroundColor = InColor; }
        const glm::vec4& GetBackgroundColor() const { return BackgroundColor; }
        void SetBorderColor(const glm::vec4& InColor) { BorderColor = InColor; }

        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        float Percent = 1.0f;
        glm::vec4 FillColor{0.25f, 0.85f, 0.40f, 0.95f};
        glm::vec4 BackgroundColor{0.06f, 0.08f, 0.10f, 0.85f};
        glm::vec4 BorderColor{0.20f, 0.28f, 0.35f, 0.80f};
    };

} // namespace Leon
