#pragma once

#include "Gameplay/AHUD.hpp"
#include "UMG/UUserWidget.hpp"

namespace Leon {

    /**
     * @brief Sandbox HUD — creates the main menu widget on MainShowcase and demo PrintString.
     */
    class ASandboxHUD : public AHUD {
    public:
        ASandboxHUD() = default;
        ASandboxHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxHUD");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

    private:
        void CreateMainMenuIfNeeded();

        TRef<UUserWidget> MainMenuWidget;
        bool bPrintKeyWasDown = false;
    };

} // namespace Leon
