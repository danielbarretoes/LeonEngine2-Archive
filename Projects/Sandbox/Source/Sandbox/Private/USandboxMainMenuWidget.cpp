#include "USandboxMainMenuWidget.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    USandboxMainMenuWidget::USandboxMainMenuWidget(const std::string& InName) : UUserWidget(InName) {}

    void USandboxMainMenuWidget::Construct() {
        BuildWidgetTree();
    }

    void USandboxMainMenuWidget::BuildWidgetTree() {
        FUIRenderer::Init();

        // Compact top-right corner chip (does not block the showcase).
        constexpr float PanelW = 200.0f;
        constexpr float PanelH = 72.0f;
        constexpr float ButtonW = 176.0f;
        constexpr float ButtonH = 32.0f;
        constexpr float Margin = 12.0f;

        RootCanvas = std::make_shared<UCanvasPanel>("RootCanvas");
        RootCanvas->SetSize({1280.0f, 720.0f});
        RootCanvas->SetPosition({0.0f, 0.0f});

        auto panel = std::make_shared<UCanvasPanel>("MenuPanel");
        panel->SetBackgroundColor({0.04f, 0.05f, 0.09f, 0.78f});
        panel->SetBorder({0.30f, 0.48f, 0.72f, 0.70f}, 1.0f);
        RootCanvas->AddChild(panel, FAnchors::TopRight(),
                               FMargin(-(Margin + PanelW), Margin, Margin, -(Margin + PanelH)));

        TitleText = std::make_shared<UTextBlock>("Title");
        TitleText->SetText(bShowcaseLayout ? "Showcase" : "Night Level");
        TitleText->SetFontScale(0.85f);
        TitleText->SetColor({0.78f, 0.84f, 0.95f, 0.95f});
        TitleText->SetJustification(ETextAlignment::Left);
        TitleText->SetSize({PanelW - 16.0f, 16.0f});
        panel->AddChild(TitleText, FAnchors::TopLeft(), FMargin(10.0f, 8.0f, -190.0f, -24.0f));

        NightLevelButton = std::make_shared<UButton>("NightLevelButton");
        NightLevelButton->SetNormalColor({0.12f, 0.18f, 0.30f, 0.95f});
        NightLevelButton->SetHoveredColor({0.18f, 0.30f, 0.50f, 1.0f});
        NightLevelButton->SetPressedColor({0.08f, 0.12f, 0.20f, 1.0f});
        NightLevelButton->SetBorder({0.40f, 0.62f, 0.92f, 0.85f}, 1.0f);

        ButtonLabel = std::make_shared<UTextBlock>("NightLevelLabel");
        ButtonLabel->SetText(bShowcaseLayout ? "Open Night Level" : "Reload Night");
        ButtonLabel->SetFontScale(0.90f);
        ButtonLabel->SetColor({0.95f, 0.97f, 1.0f, 1.0f});
        ButtonLabel->SetJustification(ETextAlignment::Center);
        NightLevelButton->SetContent(ButtonLabel);

        NightLevelButton->OnClicked.AddLambda([this]() { OnOpenNightLevelClicked(); });

        const float buttonX = (PanelW - ButtonW) * 0.5f;
        panel->AddChild(NightLevelButton, FAnchors::TopLeft(),
                        FMargin(buttonX, 30.0f, -(buttonX + ButtonW), -(30.0f + ButtonH)));

        SetWidgetTree(RootCanvas);
        SetSize({1280.0f, 720.0f});
        SetPosition({0.0f, 0.0f});
    }

    void USandboxMainMenuWidget::OnOpenNightLevelClicked() {
        PrintString("Opening Night Level...", 2.0f);

        UWorld* world = nullptr;
        if (OwningPlayer) {
            world = OwningPlayer->GetWorld();
        }
        UGameplayStatics::OpenLevel(world, "/Game/Maps/NightLevel");
    }

} // namespace Leon
