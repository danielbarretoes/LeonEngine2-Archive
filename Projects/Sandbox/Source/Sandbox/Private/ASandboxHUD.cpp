#include "ASandboxHUD.hpp"
#include "USandboxRenderLabWidget.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"

namespace Leon {

    ASandboxHUD::ASandboxHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AHUD(InHandle, InWorld, InName) {}

    void ASandboxHUD::BeginPlay() {
        AHUD::BeginPlay();

        if (!PlayerController) {
            LE_CORE_ERROR("ASandboxHUD::BeginPlay: PlayerController is null");
            return;
        }

        RenderLabWidget = UUserWidget::CreateWidget<USandboxRenderLabWidget>(PlayerController);
        if (RenderLabWidget) {
            RenderLabWidget->AddToViewport(30);
            RenderLabWidget->SetLabVisible(true);
        } else {
            PlayerController->SetInputModeGameOnly();
            PlayerController->SetShowMouseCursor(false);
        }

        PrintString("Sandbox Render Lab — H toggles panel; P reprints welcome.", 5.0f);
    }

    void ASandboxHUD::Tick(float DeltaSeconds) {
        AHUD::Tick(DeltaSeconds);

        const bool bPDown = FInput::IsKeyPressed(Key::P);
        if (bPDown && !bPrintKeyWasDown)
            PrintString("Hello from LeonEngine!", 2.0f);
        bPrintKeyWasDown = bPDown;

        const bool bHDown = FInput::IsKeyPressed(Key::H);
        if (bHDown && !bHideKeyWasDown && RenderLabWidget)
            RenderLabWidget->SetLabVisible(!RenderLabWidget->IsLabVisible());
        bHideKeyWasDown = bHDown;
    }

} // namespace Leon
