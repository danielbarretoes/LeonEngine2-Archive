#include "FLeonTournamentUILayout.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"

namespace Leon {

    ULeonTournamentGameInstance* FLeonTournamentUILayout::GI() {
        return UEngine::HasInstance()
                   ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                   : nullptr;
    }

    bool FLeonTournamentUILayout::GamepadEdge(int InButton, bool& InOutWasDown) {
        const FInputSettings& input = FInputSettings::Get();
        if (!input.bEnableGamepad || !FInput::IsGamepadConnected(input.GamepadId)) {
            InOutWasDown = false;
            return false;
        }
        const bool down = FInput::IsGamepadButtonPressed(InButton, input.GamepadId);
        const bool edge = down && !InOutWasDown;
        InOutWasDown = down;
        return edge;
    }

    ALeonTournamentGameMode* FLeonTournamentUILayout::GM(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameMode*>(world->GetGameMode()) : nullptr;
    }

    ALeonTournamentGameState* FLeonTournamentUILayout::GS(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameState*>(world->GetGameState()) : nullptr;
    }

    bool FLeonTournamentUILayout::IsClientWorld(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world && world->GetNetMode() == ENetMode::Client;
    }

    FMargin FLeonTournamentUILayout::BoxTL(float InX, float InY, float InW, float InH) {
        return FMargin(InX, InY, -(InX + InW), -(InY + InH));
    }
    FMargin FLeonTournamentUILayout::BoxBL(float InX, float InBottom, float InW, float InH) {
        return FMargin(InX, -(InBottom + InH), -(InX + InW), InBottom);
    }
    FMargin FLeonTournamentUILayout::BoxBR(float InRight, float InBottom, float InW, float InH) {
        return FMargin(-(InRight + InW), -(InBottom + InH), InRight, InBottom);
    }
    FMargin FLeonTournamentUILayout::BoxTC(float InTop, float InW, float InH, float InOx) {
        return FMargin(InOx - InW * 0.5f, InTop, -(InOx + InW * 0.5f), -(InTop + InH));
    }
    FMargin FLeonTournamentUILayout::BoxBC(float InBottom, float InW, float InH, float InOx) {
        return FMargin(InOx - InW * 0.5f, -(InBottom + InH), -(InOx + InW * 0.5f), InBottom);
    }
    FMargin FLeonTournamentUILayout::BoxC(float InOx, float InOy, float InW, float InH) {
        return FMargin(InOx - InW * 0.5f, InOy - InH * 0.5f, -(InOx + InW * 0.5f), -(InOy + InH * 0.5f));
    }

    glm::vec2 FLeonTournamentUILayout::MeasurePadded(const std::string& InText, float InScale, float InPadX,
                                                     float InPadY) {
        const glm::vec2 m = FUIRenderer::MeasureString(InText, InScale);
        return {m.x + InPadX * 2.0f, m.y + InPadY * 2.0f};
    }

    TRef<UButton> FLeonTournamentUILayout::MakeButton(const std::string& InName, const std::string& InLabel,
                                                      float InFont, float InMinW, float InMinH) {
        FUIRenderer::Init();
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

    void FLeonTournamentUILayout::PlaceButtonTL(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InX, float InY) {
        const glm::vec2 s = InBtn->GetSize();
        InRoot.AddChild(InBtn, FAnchors::TopLeft(), BoxTL(InX, InY, s.x, s.y));
    }
    void FLeonTournamentUILayout::PlaceButtonC(UCanvasPanel& InRoot, const TRef<UButton>& InBtn, float InOx, float InOy) {
        const glm::vec2 s = InBtn->GetSize();
        InRoot.AddChild(InBtn, FAnchors::Center(), BoxC(InOx, InOy, s.x, s.y));
    }
    void FLeonTournamentUILayout::PlaceTextTL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX, float InY,
                                              float InMinW, float InMinH) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::TopLeft(), BoxTL(InX, InY, std::max(InMinW, e.x), std::max(InMinH, e.y)));
    }
    void FLeonTournamentUILayout::PlaceTextTC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InTop,
                                              float InMinW, float InOx) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::TopCenter(), BoxTC(InTop, std::max(InMinW, e.x), e.y, InOx));
    }
    void FLeonTournamentUILayout::PlaceTextBL(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InX,
                                              float InBottom, float InMinW) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomLeft(), BoxBL(InX, InBottom, std::max(InMinW, e.x), e.y));
    }
    void FLeonTournamentUILayout::PlaceTextBR(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InRight,
                                              float InBottom, float InMinW) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomRight(), BoxBR(InRight, InBottom, std::max(InMinW, e.x), e.y));
    }
    void FLeonTournamentUILayout::PlaceTextBC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InBottom,
                                              float InMinW) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::BottomCenter(), BoxBC(InBottom, std::max(InMinW, e.x), e.y));
    }
    void FLeonTournamentUILayout::PlaceTextC(UCanvasPanel& InRoot, const TRef<UTextBlock>& InText, float InOx, float InOy,
                                             float InMinW, float InMinH) {
        const glm::vec2 e = MeasurePadded(InText->GetText().empty() ? " " : InText->GetText(), InText->GetFontScale());
        InRoot.AddChild(InText, FAnchors::Center(),
                        BoxC(InOx, InOy, std::max(InMinW, e.x), std::max(InMinH, e.y)));
    }

} // namespace Leon
