#pragma once

#include "UMG/FUITypeScale.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <string>
#include <glm/glm.hpp>

namespace Leon {

    /**
     * @brief Measured-layout helpers for Inter-baked 48px UI (product-agnostic).
     * Game-specific GI/GM accessors stay in the project wrapper.
     */
    struct FUILayout {
        static constexpr float kFsCaption = FUITypeScale::Small;
        static constexpr float kFsBody = FUITypeScale::P;
        static constexpr float kFsLabel = FUITypeScale::H3;
        static constexpr float kFsButton = FUITypeScale::Button;
        static constexpr float kFsSub = FUITypeScale::H3;
        static constexpr float kFsTitle = FUITypeScale::H2;
        static constexpr float kFsHero = FUITypeScale::H1;
        static constexpr float kFsScore = FUITypeScale::Score;
        static constexpr float kFsTimer = FUITypeScale::Timer;
        static constexpr float kFsVital = FUITypeScale::Vital;
        static constexpr float kFsBanner = FUITypeScale::Banner;
        static constexpr float kDesignWidth = 1280.0f;
        static constexpr float kDesignHeight = 720.0f;

        /** Client window size in pixels (falls back to design). */
        static glm::vec2 ResolveWindowSize();

        /** Uniform scale vs the 1280×720 design; clamped so 800×600 stays readable. */
        static float LayoutScale(float InViewportW, float InViewportH) {
            const float sx = InViewportW / kDesignWidth;
            const float sy = InViewportH / kDesignHeight;
            return std::clamp(std::min(sx, sy), 0.7f, 2.25f);
        }

        /**
         * Like LayoutScale, but never grows taller than the viewport for a given design stack height.
         * Prevents menus from pushing buttons off-screen at 1080p (or dense 720p layouts).
         */
        static float LayoutScaleFit(float InViewportW, float InViewportH, float InDesignContentHeight,
                                    float InVerticalMargin = 96.0f) {
            float scale = LayoutScale(InViewportW, InViewportH);
            if (InDesignContentHeight > 1.0f) {
                const float avail = std::max(InViewportH - InVerticalMargin, 120.0f);
                scale = std::min(scale, avail / InDesignContentHeight);
            }
            return std::clamp(scale, 0.55f, 2.25f);
        }

        static float LayoutScaleForWindow() {
            const glm::vec2 vp = ResolveWindowSize();
            return LayoutScale(vp.x, vp.y);
        }

        /** Prefer window size; fall back to canvas size then design. */
        static glm::vec2 ResolveViewportSize(const UCanvasPanel* InRoot);

        /**
         * Scale root slots/fonts from last AppliedScale to the current window scale.
         * When InDesignContentHeight > 0, uses LayoutScaleFit so tall menus stay on-screen.
         */
        static float SyncResolutionScale(UCanvasPanel& InRoot, float& InOutAppliedScale, glm::vec2& InOutAppliedViewport,
                                         float InDesignContentHeight = 0.0f, float InVerticalMargin = 96.0f);

        /** True on rising edge of a gamepad button (respects FInputSettings). */
        static bool GamepadEdge(int InButton, bool& InOutWasDown);

        static FMargin BoxTL(float InX, float InY, float InW, float InH);
        static FMargin BoxBL(float InX, float InBottom, float InW, float InH);
        static FMargin BoxBR(float InRight, float InBottom, float InW, float InH);
        static FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f);
        static FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f);
        static FMargin BoxC(float InOx, float InOy, float InW, float InH);
        static FMargin BoxTopStretch(float InTop, float InH, float InLeft = 0.0f, float InRight = 0.0f);
        static FMargin BoxLeftStretch(float InLeft, float InW, float InTop = 0.0f, float InBottom = 0.0f);

        static glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f,
                                       float InPadY = 6.0f);
        static TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel, float InFont = kFsButton,
                                        float InMinW = 200.0f, float InMinH = 0.0f);

        static void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY);
        static void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy);
        static void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                                float InMinW = 0.0f, float InMinH = 0.0f);
        static void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW = 0.0f,
                                float InOx = 0.0f);
        static void PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                                float InMinW = 0.0f);
        static void PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                                float InMinW = 0.0f);
        static void PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom,
                                float InMinW = 0.0f);
        static void PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                               float InMinW = 0.0f, float InMinH = 0.0f);
        static void PlaceWidgetTL(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InX, float InY);
        static void PlaceWidgetBL(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InX, float InBottom);
        static void PlaceWidgetBR(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InRight, float InBottom);
    };

} // namespace Leon
