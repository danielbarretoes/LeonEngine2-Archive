#include "UMG/FUILayout.hpp"

namespace Leon {

    FMargin FUILayout::BoxTL(float InX, float InY, float InW, float InH) {
        return FMargin(InX, InY, -(InX + InW), -(InY + InH));
    }
    FMargin FUILayout::BoxBL(float InX, float InBottom, float InW, float InH) {
        return FMargin(InX, -(InBottom + InH), -(InX + InW), InBottom);
    }
    FMargin FUILayout::BoxBR(float InRight, float InBottom, float InW, float InH) {
        return FMargin(-(InRight + InW), -(InBottom + InH), InRight, InBottom);
    }
    FMargin FUILayout::BoxTC(float InTop, float InW, float InH, float InOx) {
        return FMargin(InOx - InW * 0.5f, InTop, -(InOx + InW * 0.5f), -(InTop + InH));
    }
    FMargin FUILayout::BoxBC(float InBottom, float InW, float InH, float InOx) {
        return FMargin(InOx - InW * 0.5f, -(InBottom + InH), -(InOx + InW * 0.5f), InBottom);
    }
    FMargin FUILayout::BoxC(float InOx, float InOy, float InW, float InH) {
        return FMargin(InOx - InW * 0.5f, InOy - InH * 0.5f, -(InOx + InW * 0.5f), -(InOy + InH * 0.5f));
    }
    FMargin FUILayout::BoxTopStretch(float InTop, float InH, float InLeft, float InRight) {
        return FMargin(InLeft, InTop, InRight, -(InTop + InH));
    }
    FMargin FUILayout::BoxLeftStretch(float InLeft, float InW, float InTop, float InBottom) {
        return FMargin(InLeft, InTop, -(InLeft + InW), InBottom);
    }

    glm::vec2 FUILayout::MeasurePadded(const std::string& InText, float InScale, float InPadX, float InPadY) {
        const glm::vec2 m = FUIRenderer::MeasureString(InText, InScale);
        return {m.x + InPadX * 2.0f, m.y + InPadY * 2.0f};
    }

    TRef<UButton> FUILayout::MakeButton(const std::string& InName, const std::string& InLabel, float InFont,
                                        float InMinW, float InMinH) {
        auto btn = std::make_shared<UButton>(InName);
        btn->SetNormalColor({0.12f, 0.18f, 0.30f, 0.95f});
        btn->SetHoveredColor({0.18f, 0.30f, 0.50f, 1.0f});
        btn->SetPressedColor({0.08f, 0.12f, 0.20f, 1.0f});
        auto label = std::make_shared<UTextBlock>(InName + "Label");
        label->SetText(InLabel);
        label->SetFontScale(InFont);
        label->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        label->SetJustification(ETextAlignment::Center);
        const glm::vec2 m = FUIRenderer::MeasureString(InLabel, InFont);
        label->SetSize(m);
        btn->SetContent(label);
        constexpr float padX = 36.0f;
        constexpr float padY = 14.0f;
        const float w = std::max(InMinW, m.x + padX * 2.0f);
        const float h = std::max(InMinH > 0.0f ? InMinH : (m.y + padY * 2.0f), m.y + padY * 2.0f);
        btn->SetSize({w, h});
        return btn;
    }

    void FUILayout::PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
        if (!InBtn)
            return;
        const glm::vec2 s = InBtn->GetSize();
        InRoot.AddChild(InBtn, FAnchors::TopLeft(), BoxTL(InX, InY, s.x, s.y));
    }
    void FUILayout::PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
        if (!InBtn)
            return;
        const glm::vec2 s = InBtn->GetSize();
        InRoot.AddChild(InBtn, FAnchors::Center(), BoxC(InOx, InOy, s.x, s.y));
    }
    void FUILayout::PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                                float InMinW, float InMinH) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::TopLeft(), BoxTL(InX, InY, std::max(InMinW, e.x), std::max(InMinH, e.y)));
    }
    void FUILayout::PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop, float InMinW,
                                float InOx) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::TopCenter(), BoxTC(InTop, std::max(InMinW, e.x), e.y, InOx));
    }
    void FUILayout::PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InBottom,
                                float InMinW) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomLeft(), BoxBL(InX, InBottom, std::max(InMinW, e.x), e.y));
    }
    void FUILayout::PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight, float InBottom,
                                float InMinW) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomRight(), BoxBR(InRight, InBottom, std::max(InMinW, e.x), e.y));
    }
    void FUILayout::PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom, float InMinW) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomCenter(), BoxBC(InBottom, std::max(InMinW, e.x), e.y));
    }
    void FUILayout::PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                               float InMinW, float InMinH) {
        if (!InText)
            return;
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::Center(), BoxC(InOx, InOy, std::max(InMinW, e.x), std::max(InMinH, e.y)));
    }

    void FUILayout::PlaceWidgetTL(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InX, float InY) {
        if (!InWidget)
            return;
        const glm::vec2 s = InWidget->GetSize();
        InRoot.AddChild(InWidget, FAnchors::TopLeft(), BoxTL(InX, InY, s.x, s.y));
    }
    void FUILayout::PlaceWidgetBL(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InX, float InBottom) {
        if (!InWidget)
            return;
        const glm::vec2 s = InWidget->GetSize();
        InRoot.AddChild(InWidget, FAnchors::BottomLeft(), BoxBL(InX, InBottom, s.x, s.y));
    }
    void FUILayout::PlaceWidgetBR(UCanvasPanel& InRoot, const TRef<UWidget>& InWidget, float InRight, float InBottom) {
        if (!InWidget)
            return;
        const glm::vec2 s = InWidget->GetSize();
        InRoot.AddChild(InWidget, FAnchors::BottomRight(), BoxBR(InRight, InBottom, s.x, s.y));
    }

} // namespace Leon
