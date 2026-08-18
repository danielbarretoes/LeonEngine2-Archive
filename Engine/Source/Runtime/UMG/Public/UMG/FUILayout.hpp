#pragma once

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
        static constexpr float kFsCaption = 0.32f;
        static constexpr float kFsBody = 0.38f;
        static constexpr float kFsLabel = 0.42f;
        static constexpr float kFsButton = 0.42f;
        static constexpr float kFsSub = 0.48f;
        static constexpr float kFsTitle = 0.65f;
        static constexpr float kFsHero = 0.78f;
        static constexpr float kFsScore = 0.72f;
        static constexpr float kFsTimer = 0.78f;
        static constexpr float kFsVital = 0.85f;
        static constexpr float kFsBanner = 1.05f;

        static FMargin BoxTL(float InX, float InY, float InW, float InH);
        static FMargin BoxBL(float InX, float InBottom, float InW, float InH);
        static FMargin BoxBR(float InRight, float InBottom, float InW, float InH);
        static FMargin BoxTC(float InTop, float InW, float InH, float InOx = 0.0f);
        static FMargin BoxBC(float InBottom, float InW, float InH, float InOx = 0.0f);
        static FMargin BoxC(float InOx, float InOy, float InW, float InH);

        static glm::vec2 MeasurePadded(const std::string& InText, float InScale, float InPadX = 8.0f,
                                       float InPadY = 6.0f);
        static TRef<UButton> MakeButton(const std::string& InName, const std::string& InLabel,
                                        float InFont = kFsButton, float InMinW = 200.0f, float InMinH = 0.0f);

        static void PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY);
        static void PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy);
        static void PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                                float InMinW = 0.0f, float InMinH = 0.0f);
        static void PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop,
                                float InMinW = 0.0f, float InOx = 0.0f);
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
