#include "UMG/ULoadingSpinner.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    namespace {
        constexpr float kTwoPi = 6.28318530718f;
    } // namespace

    ULoadingSpinner::ULoadingSpinner(const std::string& InName) : UWidget(InName) { Size = {64.0f, 64.0f}; }

    void ULoadingSpinner::SetSegmentCount(int InCount) { SegmentCount = std::clamp(InCount, 3, 32); }

    void ULoadingSpinner::Tick(float InDeltaTime) {
        UWidget::Tick(InDeltaTime);
        if (!IsVisible())
            return;
        Phase += InDeltaTime * SpinSpeed;
        if (Phase > kTwoPi)
            Phase = std::fmod(Phase, kTwoPi);
    }

    void ULoadingSpinner::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        const glm::vec2 center = InAllottedGeometry.AbsolutePosition + InAllottedGeometry.Size * 0.5f;
        const float minDim = std::min(InAllottedGeometry.Size.x, InAllottedGeometry.Size.y);
        const float orbitRadius = std::max(4.0f, minDim * 0.5f - DotRadius - 4.0f);
        const int activeSegment = SegmentCount > 0 ? static_cast<int>(Phase / kTwoPi * SegmentCount) % SegmentCount : 0;

        for (int i = 0; i < SegmentCount; ++i) {
            const float angle = Phase + (static_cast<float>(i) / static_cast<float>(SegmentCount)) * kTwoPi;
            const glm::vec2 pos{center.x + std::cos(angle) * orbitRadius, center.y + std::sin(angle) * orbitRadius};

            const int dist = (i - activeSegment + SegmentCount) % SegmentCount;
            const float alpha = 0.15f + 0.85f * (1.0f - static_cast<float>(dist) / static_cast<float>(SegmentCount));
            glm::vec4 color = SpinnerColor;
            color.a *= alpha;

            const glm::vec2 dotMin{pos.x - DotRadius, pos.y - DotRadius};
            const glm::vec2 dotMax{pos.x + DotRadius, pos.y + DotRadius};
            FUIRenderer::DrawQuad(dotMin, dotMax, color);
        }

        if (!Label.empty()) {
            const glm::vec2 measured = FUIRenderer::MeasureString(Label, LabelFontScale);
            const float labelX = center.x;
            const float labelY = center.y + orbitRadius + DotRadius + 8.0f;
            FUIRenderer::DrawString(labelX, labelY, Label, LabelColor, LabelFontScale, ETextAlignment::Center);
            (void)measured;
        }
    }

} // namespace Leon
