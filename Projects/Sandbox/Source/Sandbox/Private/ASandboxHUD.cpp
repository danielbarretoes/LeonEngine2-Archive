#include "ASandboxHUD.hpp"
#include "USandboxMainMenuWidget.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"

namespace Leon {

    ASandboxHUD::ASandboxHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AHUD(InHandle, InWorld, InName) {}

    void ASandboxHUD::BeginPlay() {
        AHUD::BeginPlay();

        if (!PlayerController) {
            LE_CORE_ERROR("ASandboxHUD::BeginPlay: PlayerController is null — menu cannot be created");
            return;
        }

        PlayerController->SetInputModeGameAndUI();
        PlayerController->SetShowMouseCursor(true);

        PrintString("Welcome to LeonEngine — fly into the spinning pickup; look down at the floor.", 5.0f);
        CreateMainMenuIfNeeded();
    }

    void ASandboxHUD::CreateMainMenuIfNeeded() {
        if (!PlayerController)
            return;

        const std::string& mapName = UEngine::Get().GetCurrentMapName();
        const bool bIsShowcase = mapName.empty() || mapName.find("ShowcaseLevel") != std::string::npos;

        auto widget = std::make_shared<USandboxMainMenuWidget>("SandboxMainMenu");
        widget->SetIsShowcaseLayout(bIsShowcase);
        widget->SetOwningPlayer(PlayerController);
        widget->Construct();

        MainMenuWidget = widget;
        MainMenuWidget->AddToViewport(0);

        if (!bIsShowcase) {
            PrintString("Night Level — wet street uses the floor planar capture.", 4.0f);
        }

        LE_CORE_INFO("ASandboxHUD: Menu widget on viewport (map='{0}', widgets={1})", mapName,
                     GetViewportWidgets().size());
    }

    void ASandboxHUD::Tick(float DeltaSeconds) {
        AHUD::Tick(DeltaSeconds);

        const bool bPDown = FInput::IsKeyPressed(Key::P);
        if (bPDown && !bPrintKeyWasDown) {
            PrintString("Hello from LeonEngine!", 2.0f);
        }
        bPrintKeyWasDown = bPDown;
    }

} // namespace Leon
